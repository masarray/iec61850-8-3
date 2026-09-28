#include "ar61850/dms/engine.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace ar61850::dms;

namespace {
std::atomic<bool> stop_requested{false};

void handle_signal(int) {
    stop_requested.store(true, std::memory_order_release);
}

const char* event_name(RuntimeEvent::Kind kind) {
    switch (kind) {
    case RuntimeEvent::Kind::Started: return "started";
    case RuntimeEvent::Kind::Stopped: return "stopped";
    case RuntimeEvent::Kind::TransportConnected: return "connected";
    case RuntimeEvent::Kind::TransportDisconnected: return "disconnected";
    case RuntimeEvent::Kind::DecodeOrServiceError: return "protocol-error";
    case RuntimeEvent::Kind::BackpressureDrop: return "backpressure-drop";
    }
    return "event";
}

void print_usage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options]\n"
        << "  --mode listen|connect     WebSocket transport role (default listen)\n"
        << "  --host HOST               bind/remote host (default 127.0.0.1)\n"
        << "  --port PORT               bind/remote port (default 8765)\n"
        << "  --access-point NAME       WebSocket path / access point (default cp1)\n"
        << "  --no-reconnect            disable reconnect in connect mode\n"
        << "  --help                    show this help\n";
}

std::uint16_t parse_port(const std::string& text) {
    const auto value = std::stoul(text);
    if (value == 0 || value > 65535) throw std::invalid_argument("port out of range");
    return static_cast<std::uint16_t>(value);
}
} // namespace

int main(int argc, char** argv) {
    try {
        WebSocketTransportConfig transport_config;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto require_value = [&](const char* option) -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument(std::string(option) + " requires a value");
                return argv[++i];
            };

            if (arg == "--help" || arg == "-h") {
                print_usage(argv[0]);
                return 0;
            } else if (arg == "--mode") {
                const auto value = require_value("--mode");
                if (value == "listen") {
                    transport_config.mode = WebSocketMode::PassiveListen;
                } else if (value == "connect") {
                    transport_config.mode = WebSocketMode::ActiveConnect;
                } else {
                    throw std::invalid_argument("--mode must be listen or connect");
                }
            } else if (arg == "--host") {
                transport_config.host = require_value("--host");
            } else if (arg == "--port") {
                transport_config.port = parse_port(require_value("--port"));
            } else if (arg == "--access-point") {
                transport_config.access_point = require_value("--access-point");
            } else if (arg == "--no-reconnect") {
                transport_config.automatic_reconnect = false;
            } else {
                throw std::invalid_argument("unknown option: " + arg);
            }
        }

        auto transport = std::make_unique<WebSocketTransport>(transport_config);
        const auto endpoint = transport->endpoint_uri();

        ServerRuntime runtime{
            ServerCore{IedModel::make_ft20_reference_model()},
            std::move(transport)
        };

        runtime.set_event_handler([](const RuntimeEvent& event) {
            std::cout << "[dms] " << event_name(event.kind);
            if (!event.detail.empty()) std::cout << " | " << event.detail;
            std::cout << std::endl;
        });

        std::signal(SIGINT, handle_signal);
#ifdef SIGTERM
        std::signal(SIGTERM, handle_signal);
#endif

        runtime.start();

        std::cout
            << "AR61850 native IEC 61850-8-3 laboratory server\n"
            << "  endpoint : " << endpoint << "\n"
            << "  profile  : iec61850-tpaa-ber-v1\n"
            << "  model    : IED1 / LD0\n"
            << "  runtime  : native C++20, no Netbeheer/Python dependency\n"
            << "Press Ctrl+C to stop.\n";

        while (!stop_requested.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        runtime.stop();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << std::endl;
        return 1;
    }
}
