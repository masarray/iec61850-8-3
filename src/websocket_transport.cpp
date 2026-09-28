#include "ar61850/dms/websocket_transport.hpp"

#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
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
            std::scoped_lock lock(peer_mutex_);
            passive_peer_ = nullptr;
        }
        connected_.store(false, std::memory_order_release);
    }

    bool send(Bytes payload) {
        if (!started_.load(std::memory_order_acquire) || payload.empty()) return false;
        if (payload.size() > config_.max_message_size) return false;

        if (config_.mode == WebSocketMode::ActiveConnect) {
            if (!client_ || !connected_.load(std::memory_order_acquire)) return false;
            return client_->sendBinary(payload).success;
        }

        ix::WebSocket* target = nullptr;
        {
            std::scoped_lock lock(peer_mutex_);
            target = passive_peer_;
        }
        if (!target || !server_) return false;

        // getClients() returns shared_ptr copies, keeping the target alive for this send.
        const auto clients = server_->getClients();
        for (const auto& client : clients) {
            if (client.get() == target) return client->sendBinary(payload).success;
        }
        return false;
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
            [this](std::shared_ptr<ix::ConnectionState> state,
                   ix::WebSocket& websocket,
                   const ix::WebSocketMessagePtr& message) {
                (void) state;
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
            case ix::WebSocketMessageType::Open:
                connected_.store(true, std::memory_order_release);
                emit_state(true, "connected " + endpoint_uri());
                break;
            case ix::WebSocketMessageType::Close:
                connected_.store(false, std::memory_order_release);
                emit_state(false, "closed: " + message->closeInfo.reason);
                break;
            case ix::WebSocketMessageType::Error:
                connected_.store(false, std::memory_order_release);
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

            {
                std::scoped_lock lock(peer_mutex_);
                passive_peer_ = &websocket;
            }
            connected_.store(true, std::memory_order_release);
            emit_state(true, "client connected at " + message->openInfo.uri);
            break;
        }
        case ix::WebSocketMessageType::Close: {
            {
                std::scoped_lock lock(peer_mutex_);
                if (passive_peer_ == &websocket) passive_peer_ = nullptr;
            }
            connected_.store(false, std::memory_order_release);
            emit_state(false, "client disconnected: " + message->closeInfo.reason);
            break;
        }
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

    std::mutex peer_mutex_;
    ix::WebSocket* passive_peer_{nullptr};

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
