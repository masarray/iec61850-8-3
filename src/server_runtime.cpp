#include "ar61850/dms/server_runtime.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace ar61850::dms {

ServerRuntime::ServerRuntime(
    ServerCore core,
    std::unique_ptr<IWireTransport> transport,
    std::size_t max_pending_messages,
    std::size_t trace_capacity)
    : core_(std::move(core)),
      transport_(std::move(transport)),
      service_worker_(max_pending_messages),
      trace_(trace_capacity) {
    if (!transport_) throw std::invalid_argument("ServerRuntime requires a transport");

    transport_->set_receive_handler([this](Bytes payload) {
        on_receive(std::move(payload));
    });

    transport_->set_state_handler([this](bool connected, std::string_view detail) {
        transport_connected_.store(connected, std::memory_order_release);
        emit(
            connected ? RuntimeEvent::Kind::TransportConnected
                      : RuntimeEvent::Kind::TransportDisconnected,
            std::string(detail));
    });
}

ServerRuntime::~ServerRuntime() { stop(); }

void ServerRuntime::set_event_handler(EventHandler handler) {
    event_handler_ = std::move(handler);
}

void ServerRuntime::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    try {
        transport_->start();
        emit(RuntimeEvent::Kind::Started);
    } catch (...) {
        running_.store(false, std::memory_order_release);
        throw;
    }
}

void ServerRuntime::stop() noexcept {
    if (!running_.exchange(false, std::memory_order_acq_rel)) return;
    try {
        transport_->stop();
    } catch (...) {
    }
    service_worker_.stop();
    transport_connected_.store(false, std::memory_order_release);
    emit(RuntimeEvent::Kind::Stopped);
}

void ServerRuntime::on_receive(Bytes payload) {
    if (!running()) return;
    trace_wire(Direction::Rx, payload.size());

    const bool accepted = service_worker_.post([this, payload = std::move(payload)]() mutable {
        try {
            auto response = core_.handle(payload);
            if (!response || response->empty()) return;

            const auto byte_count = response->size();
            if (transport_->send(std::move(*response))) {
                trace_wire(Direction::Tx, byte_count);
            } else {
                emit(RuntimeEvent::Kind::TransportDisconnected, "transport rejected send");
            }
        } catch (const std::exception& ex) {
            emit(RuntimeEvent::Kind::DecodeOrServiceError, ex.what());
        } catch (...) {
            emit(RuntimeEvent::Kind::DecodeOrServiceError, "unknown protocol exception");
        }
    });

    if (!accepted) {
        dropped_messages_.fetch_add(1, std::memory_order_relaxed);
        emit(RuntimeEvent::Kind::BackpressureDrop, "service worker queue is full");
    }
}

void ServerRuntime::emit(RuntimeEvent::Kind kind, std::string detail) {
    if (event_handler_) event_handler_(RuntimeEvent{kind, std::move(detail)});
}

void ServerRuntime::trace_wire(Direction direction, std::size_t bytes) {
    TraceEvent event;
    event.observed_at = std::chrono::system_clock::now();
    event.endpoint = EndpointRole::Server;
    event.direction = direction;
    event.byte_count = bytes;
    trace_.push(std::move(event));
}

} // namespace ar61850::dms
