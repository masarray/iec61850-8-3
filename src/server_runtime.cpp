#include "ar61850/dms/server_runtime.hpp"

#include <algorithm>
#include <exception>
#include <future>
#include <stdexcept>
#include <utility>

namespace ar61850::dms {

ServerRuntime::ServerRuntime(
    ServerCore core,
    std::unique_ptr<IWireTransport> transport,
    std::size_t max_pending_messages,
    std::size_t trace_capacity,
    std::size_t report_capacity)
    : core_(std::move(core)),
      transport_(std::move(transport)),
      service_worker_(max_pending_messages),
      trace_(trace_capacity),
      report_capacity_(std::max<std::size_t>(1, report_capacity)) {
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

        // Start() is a readiness contract, not merely a thread-launch request.
        // Establish a service-worker barrier before the timer can enqueue any
        // scheduled-report work. This removes startup ordering from transport
        // callbacks and makes the headless runtime deterministic under CI load.
        auto ready_promise = std::make_shared<std::promise<void>>();
        auto ready_future = ready_promise->get_future();
        if (!service_worker_.post([ready_promise] {
                ready_promise->set_value();
            }) ||
            ready_future.wait_for(std::chrono::seconds{3}) !=
                std::future_status::ready) {
            throw std::runtime_error(
                "service worker did not become ready during runtime start");
        }

        report_scheduler_ = std::jthread([this](std::stop_token token) {
            using namespace std::chrono_literals;
            while (!token.stop_requested()) {
                std::this_thread::sleep_for(25ms);
                if (token.stop_requested() || !running()) break;
                if (report_tick_pending_.exchange(
                        true, std::memory_order_acq_rel)) {
                    continue;
                }
                const bool accepted = service_worker_.post([this] {
                    core_.poll_scheduled_reports();
                    flush_unconfirmed();
                    report_tick_pending_.store(
                        false, std::memory_order_release);
                });
                if (!accepted) {
                    report_tick_pending_.store(
                        false, std::memory_order_release);
                }
            }
        });
        emit(RuntimeEvent::Kind::Started);
    } catch (...) {
        running_.store(false, std::memory_order_release);
        throw;
    }
}

void ServerRuntime::stop() noexcept {
    if (!running_.exchange(false, std::memory_order_acq_rel)) return;
    if (report_scheduler_.joinable()) {
        report_scheduler_.request_stop();
        report_scheduler_.join();
    }
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
                const bool ok = core_.set_float(owned_reference, value);
                if (ok) flush_unconfirmed();
                promise->set_value(ok);
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

bool ServerRuntime::set_float_batch(
    std::vector<std::pair<std::string, float>> updates,
    std::chrono::milliseconds timeout) {
    if (!running() || updates.empty()) return false;

    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    const bool accepted = service_worker_.post(
        [this, promise, updates = std::move(updates)]() mutable {
            try {
                const bool ok = core_.set_float_batch(updates);
                if (ok) flush_unconfirmed();
                promise->set_value(ok);
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

bool ServerRuntime::set_quality(
    std::string_view reference,
    Quality quality,
    std::chrono::milliseconds timeout) {
    if (!running()) return false;

    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    const std::string owned_reference(reference);
    const bool accepted = service_worker_.post(
        [this, promise, owned_reference, quality] {
            try {
                const bool ok =
                    core_.set_quality(owned_reference, quality);
                if (ok) flush_unconfirmed();
                promise->set_value(ok);
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

std::vector<ObservedReport> ServerRuntime::report_snapshot() const {
    std::scoped_lock lock(report_mutex_);
    return std::vector<ObservedReport>(reports_.begin(), reports_.end());
}

void ServerRuntime::clear_reports() {
    std::scoped_lock lock(report_mutex_);
    reports_.clear();
}

void ServerRuntime::record_report(const ReportPdu& report) {
    std::scoped_lock lock(report_mutex_);
    if (reports_.size() >= report_capacity_) reports_.pop_front();
    reports_.push_back(ObservedReport{
        .sequence = next_report_sequence_++,
        .observed_at = std::chrono::system_clock::now(),
        .report = report
    });
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
            flush_unconfirmed();
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

void ServerRuntime::flush_unconfirmed() {
    auto pending = core_.drain_unconfirmed();
    for (auto& pdu : pending) {
        auto wire = codec_.encode(pdu);
        const auto byte_count = wire.size();
        if (transport_->send(std::move(wire))) {
            trace_wire(Direction::Tx, pdu, byte_count);
            if (pdu.service == ServiceKind::Report &&
                std::holds_alternative<ReportPdu>(pdu.payload)) {
                record_report(std::get<ReportPdu>(pdu.payload));
            }
        } else {
            emit(
                RuntimeEvent::Kind::TransportDisconnected,
                "transport rejected unconfirmed report");
            break;
        }
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
