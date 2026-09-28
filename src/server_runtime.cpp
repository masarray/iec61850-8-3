#include "ar61850/dms/server_runtime.hpp"

#include <exception>
#include <future>
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
        if (!start_transport()) throw std::runtime_error("unable to start transport");
        emit(RuntimeEvent::Kind::Started);
    } catch (...) {
        running_.store(false, std::memory_order_release);
        throw;
    }
}

void ServerRuntime::stop() noexcept {
    if (!running_.exchange(false, std::memory_order_acq_rel)) return;
    stop_transport();
    service_worker_.stop();
    emit(RuntimeEvent::Kind::Stopped);
}

bool ServerRuntime::start_transport() {
    if (!running()) return false;
    std::scoped_lock lock(transport_mutex_);
    if (transport_active_.load(std::memory_order_acquire)) return true;
    try {
        transport_->start();
        transport_active_.store(true, std::memory_order_release);
        return true;
    } catch (...) {
        transport_active_.store(false, std::memory_order_release);
        transport_connected_.store(false, std::memory_order_release);
        throw;
    }
}

bool ServerRuntime::stop_transport() noexcept {
    std::scoped_lock lock(transport_mutex_);
    if (!transport_active_.exchange(false, std::memory_order_acq_rel)) return true;
    try {
        transport_->stop();
    } catch (...) {
        return false;
    }
    transport_connected_.store(false, std::memory_order_release);
    return true;
}

std::optional<IedModel> ServerRuntime::model_snapshot(std::chrono::milliseconds timeout) {
    if (!running()) return std::nullopt;

    auto promise = std::make_shared<std::promise<IedModel>>();
    auto future = promise->get_future();
    const bool accepted = service_worker_.post([this, promise] {
        try {
            promise->set_value(core_.model_snapshot());
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    });
    if (!accepted) return std::nullopt;
    if (future.wait_for(timeout) != std::future_status::ready) return std::nullopt;
    try {
        return future.get();
    } catch (...) {
        return std::nullopt;
    }
}

bool ServerRuntime::set_float(
    std::string_view reference,
    float value,
    std::chrono::milliseconds timeout) {
    if (!running()) return false;

    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    const std::string owned_reference(reference);
    const bool accepted = service_worker_.post(
        [this, promise, owned_reference, value] {
            try {
                promise->set_value(core_.set_float(owned_reference, value));
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
    if (!accepted) return false;
    if (future.wait_for(timeout) != std::future_status::ready) return false;
    try {
        return future.get();
    } catch (...) {
        return false;
    }
}

void ServerRuntime::on_receive(Bytes payload) {
    if (!running()) return;

    const bool accepted = service_worker_.post([this, payload = std::move(payload)]() mutable {
        try {
            const auto request = codec_.decode(payload);
            trace_wire(Direction::Rx, request, payload.size());

            auto response = core_.handle_pdu(request);
            if (!response) return;

            auto wire = codec_.encode(*response);
            const auto byte_count = wire.size();
            if (transport_->send(std::move(wire))) {
                trace_wire(Direction::Tx, *response, byte_count);
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

void ServerRuntime::trace_wire(Direction direction, const DmsPdu& pdu, std::size_t bytes) {
    TraceEvent event;
    event.observed_at = std::chrono::system_clock::now();
    event.endpoint = EndpointRole::Server;
    event.direction = direction;
    event.message_class = pdu.message_class;
    event.service = pdu.service;
    event.byte_count = bytes;
    event.invoke_id = pdu.invoke_id;
    event.associate_id = pdu.associate_id;
    trace_.push(std::move(event));
}

} // namespace ar61850::dms
