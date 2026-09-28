#include "ar61850/dms/websocket_transport.hpp"

#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ar61850::dms {
namespace {

void ensure_network_initialized() {
    static std::once_flag once;
    std::call_once(once, [] {
        ix::initNetSystem();
        std::atexit([] { ix::uninitNetSystem(); });
    });
}

std::string normalized_path(const std::string& access_point) {
    if (access_point.empty()) return "/";
    if (access_point.front() == '/') return access_point;
    return "/" + access_point;
}

Bytes to_bytes(const std::string& payload) {
    return Bytes(payload.begin(), payload.end());
}

} // namespace

class WebSocketTransport::Impl {
public:
    explicit Impl(WebSocketTransportConfig config) : config_(std::move(config)) {
        if (config_.host.empty()) throw std::invalid_argument("WebSocket host cannot be empty");
        if (config_.mode == WebSocketMode::ActiveConnect && config_.port == 0) {
            throw std::invalid_argument("active WebSocket port must be non-zero");
        }
        if (config_.max_message_size == 0) {
            throw std::invalid_argument("WebSocket max_message_size must be non-zero");
        }
    }

    ~Impl() { stop(); }

    void set_receive_handler(ReceiveHandler handler) {
        std::scoped_lock lock(handler_mutex_);
        receive_handler_ = std::move(handler);
    }

    void set_state_handler(StateHandler handler) {
        std::scoped_lock lock(handler_mutex_);
        state_handler_ = std::move(handler);
    }

    const WebSocketTransportConfig& config() const noexcept { return config_; }

    std::string endpoint_uri() const {
        return "ws://" + config_.host + ":" + std::to_string(config_.port) +
            normalized_path(config_.access_point);
    }

    void start() {
        bool expected = false;
        if (!started_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;

        try {
            ensure_network_initialized();
            if (config_.mode == WebSocketMode::PassiveListen) {
                start_passive();
            } else {
                start_active();
            }
        } catch (...) {
            started_.store(false, std::memory_order_release);
            throw;
        }
    }

    void stop() noexcept {
        if (!started_.exchange(false, std::memory_order_acq_rel)) return;

        try {
            if (server_) server_->stop();
        } catch (...) {
        }

        try {
            if (client_) client_->stop();
        } catch (...) {
        }

        {
            std::scoped_lock lock(readiness_mutex_);
            application_ready_ = false;
        }
        connected_.store(false, std::memory_order_release);
        readiness_cv_.notify_all();
    }

    bool send(Bytes payload) {
        if (!started_.load(std::memory_order_acquire) || payload.empty()) return false;
        if (payload.size() > config_.max_message_size) return false;

        if (config_.mode == WebSocketMode::ActiveConnect) {
            if (!client_ || !connected_.load(std::memory_order_acquire)) return false;
            {
                std::unique_lock lock(readiness_mutex_);
                const auto ready = readiness_cv_.wait_for(
                    lock,
                    std::chrono::milliseconds(config_.ready_probe_timeout_ms),
                    [this] {
                        return application_ready_ ||
                            !started_.load(std::memory_order_acquire) ||
                            !client_ ||
                            client_->getReadyState() != ix::ReadyState::Open;
                    });
                if (!ready || !application_ready_) return false;
            }
            if (!connected_.load(std::memory_order_acquire)) return false;
            const std::string wire(
                reinterpret_cast<const char*>(payload.data()), payload.size());
            const auto info = client_->sendBinary(wire);
            if (!info.success) return false;

            // IXWebSocket clients use a non-blocking send path. For a protocol
            // request API, returning success while bytes still sit only in the
            // library queue creates a platform-dependent race (notably on
            // Windows CI). Wait briefly for the library buffer to reach the OS.
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (client_->bufferedAmount() != 0 &&
                   std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return client_->bufferedAmount() == 0;
        }

        if (!server_ || !connected_.load(std::memory_order_acquire)) return false;
        const auto clients = server_->getClients();
        if (clients.empty()) return false;
        const auto& peer = *clients.begin();
        if (!peer) return false;
        const std::string wire(
            reinterpret_cast<const char*>(payload.data()), payload.size());
        return peer->sendBinary(wire).success;
    }

private:
    void start_passive() {
        if (config_.port == 0) {
            const auto free_port = ix::getFreePort();
            if (free_port <= 0 || free_port > 65535) {
                throw std::runtime_error("unable to allocate a free WebSocket port");
            }
            config_.port = static_cast<std::uint16_t>(free_port);
        }

        server_ = std::make_unique<ix::WebSocketServer>(
            static_cast<int>(config_.port),
            config_.host,
            ix::SocketServer::kDefaultTcpBacklog,
            1);

        server_->disablePerMessageDeflate();
        server_->setLogCallback([this](ix::LogLevel level, const std::string& message) {
            if (level == ix::LogLevel::Error) emit_state(false, message);
        });

        server_->setOnClientMessageCallback(
            [this](std::shared_ptr<ix::ConnectionState>,
                   ix::WebSocket& websocket,
                   const ix::WebSocketMessagePtr& message) {
                handle_passive_message(websocket, message);
            });

        const auto listen = server_->listen();
        if (!listen.first) {
            server_.reset();
            throw std::runtime_error("WebSocket listen failed: " + listen.second);
        }
        server_->start();
    }

    void start_active() {
        client_ = std::make_unique<ix::WebSocket>();
        client_->setUrl(endpoint_uri());
        client_->disablePerMessageDeflate();
        if (!config_.subprotocol.empty()) client_->addSubProtocol(config_.subprotocol);
        if (config_.automatic_reconnect) {
            client_->enableAutomaticReconnection();
        } else {
            client_->disableAutomaticReconnection();
        }

        client_->setOnMessageCallback([this](const ix::WebSocketMessagePtr& message) {
            if (!message) return;

            switch (message->type) {
            case ix::WebSocketMessageType::Open: {
                {
                    std::scoped_lock lock(readiness_mutex_);
                    application_ready_ = false;
                }
                const auto probe = client_->ping("ar61850-ready");
                if (!probe.success) {
                    connected_.store(false, std::memory_order_release);
                    emit_state(false, "WebSocket open but readiness probe send failed");
                }
                break;
            }
            case ix::WebSocketMessageType::Pong:
                if (message->str == "ar61850-ready") {
                    {
                        std::scoped_lock lock(readiness_mutex_);
                        application_ready_ = true;
                    }
                    connected_.store(true, std::memory_order_release);
                    readiness_cv_.notify_all();
                    emit_state(true, "connected " + endpoint_uri());
                }
                break;
            case ix::WebSocketMessageType::Close:
                {
                    std::scoped_lock lock(readiness_mutex_);
                    application_ready_ = false;
                }
                connected_.store(false, std::memory_order_release);
                readiness_cv_.notify_all();
                emit_state(false, "closed: " + message->closeInfo.reason);
                break;
            case ix::WebSocketMessageType::Error:
                {
                    std::scoped_lock lock(readiness_mutex_);
                    application_ready_ = false;
                }
                connected_.store(false, std::memory_order_release);
                readiness_cv_.notify_all();
                emit_state(false, "error: " + message->errorInfo.reason);
                break;
            case ix::WebSocketMessageType::Message:
                if (!message->binary) {
                    client_->close(1003, "IEC 61850 BER requires binary WebSocket frames");
                    return;
                }
                if (message->str.size() > config_.max_message_size) {
                    client_->close(1009, "IEC 61850 message exceeds configured limit");
                    return;
                }
                emit_receive(to_bytes(message->str));
                break;
            default:
                break;
            }
        });

        client_->start();
    }

    void handle_passive_message(
        ix::WebSocket& websocket,
        const ix::WebSocketMessagePtr& message) {
        if (!message) return;

        switch (message->type) {
        case ix::WebSocketMessageType::Open: {
            const auto expected_path = normalized_path(config_.access_point);
            if (!config_.access_point.empty() && message->openInfo.uri != expected_path) {
                websocket.close(1008, "unknown access point");
                return;
            }

            connected_.store(true, std::memory_order_release);
            emit_state(true, "client connected at " + message->openInfo.uri);
            break;
        }
        case ix::WebSocketMessageType::Close:
            connected_.store(false, std::memory_order_release);
            emit_state(false, "client disconnected: " + message->closeInfo.reason);
            break;
        case ix::WebSocketMessageType::Error:
            emit_state(false, "client error: " + message->errorInfo.reason);
            break;
        case ix::WebSocketMessageType::Message:
            if (!message->binary) {
                websocket.close(1003, "IEC 61850 BER requires binary WebSocket frames");
                return;
            }
            if (message->str.size() > config_.max_message_size) {
                websocket.close(1009, "IEC 61850 message exceeds configured limit");
                return;
            }
            emit_receive(to_bytes(message->str));
            break;
        default:
            break;
        }
    }

    void emit_receive(Bytes payload) {
        ReceiveHandler handler;
        {
            std::scoped_lock lock(handler_mutex_);
            handler = receive_handler_;
        }
        if (handler) handler(std::move(payload));
    }

    void emit_state(bool connected, const std::string& detail) {
        StateHandler handler;
        {
            std::scoped_lock lock(handler_mutex_);
            handler = state_handler_;
        }
        if (handler) handler(connected, detail);
    }

    WebSocketTransportConfig config_;
    std::atomic<bool> started_{false};
    std::atomic<bool> connected_{false};

    std::unique_ptr<ix::WebSocketServer> server_;
    std::unique_ptr<ix::WebSocket> client_;

    std::mutex readiness_mutex_;
    std::condition_variable readiness_cv_;
    bool application_ready_{false};

    std::mutex handler_mutex_;
    ReceiveHandler receive_handler_;
    StateHandler state_handler_;
};

WebSocketTransport::WebSocketTransport(WebSocketTransportConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

WebSocketTransport::~WebSocketTransport() = default;

void WebSocketTransport::set_receive_handler(ReceiveHandler handler) {
    impl_->set_receive_handler(std::move(handler));
}

void WebSocketTransport::set_state_handler(StateHandler handler) {
    impl_->set_state_handler(std::move(handler));
}

void WebSocketTransport::start() { impl_->start(); }
void WebSocketTransport::stop() noexcept { impl_->stop(); }
bool WebSocketTransport::send(Bytes payload) { return impl_->send(std::move(payload)); }

const WebSocketTransportConfig& WebSocketTransport::config() const noexcept {
    return impl_->config();
}

std::string WebSocketTransport::endpoint_uri() const { return impl_->endpoint_uri(); }

} // namespace ar61850::dms
