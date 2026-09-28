#pragma once

#include "ar61850/dms/server_core.hpp"
#include "ar61850/dms/trace.hpp"
#include "ar61850/dms/transport.hpp"
#include "ar61850/dms/worker.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace ar61850::dms {

struct RuntimeEvent {
    enum class Kind : std::uint8_t {
        Started,
        Stopped,
        TransportConnected,
        TransportDisconnected,
        DecodeOrServiceError,
        BackpressureDrop
    };

    Kind kind{Kind::Started};
    std::string detail;
};

class ServerRuntime {
public:
    using EventHandler = std::function<void(const RuntimeEvent&)>;

    ServerRuntime(
        ServerCore core,
        std::unique_ptr<IWireTransport> transport,
        std::size_t max_pending_messages = 1024,
        std::size_t trace_capacity = 4096);

    ~ServerRuntime();

    ServerRuntime(const ServerRuntime&) = delete;
    ServerRuntime& operator=(const ServerRuntime&) = delete;

    void set_event_handler(EventHandler handler);
    void start();
    void stop() noexcept;
    bool start_transport();
    bool stop_transport() noexcept;

    bool running() const noexcept { return running_.load(std::memory_order_acquire); }
    bool transport_active() const noexcept {
        return transport_active_.load(std::memory_order_acquire);
    }
    bool transport_connected() const noexcept {
        return transport_connected_.load(std::memory_order_acquire);
    }
    std::uint64_t dropped_messages() const noexcept {
        return dropped_messages_.load(std::memory_order_relaxed);
    }

    std::vector<TraceEvent> trace_snapshot() const { return trace_.snapshot(); }
    void clear_trace() { trace_.clear(); }

    std::optional<IedModel> model_snapshot(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{1000});
    bool set_float(
        std::string_view reference,
        float value,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{1000});

private:
    void on_receive(Bytes payload);
    void emit(RuntimeEvent::Kind kind, std::string detail = {});
    void trace_wire(Direction direction, const DmsPdu& pdu, std::size_t bytes);

    ServerCore core_;
    ProtocolCodec codec_;
    std::unique_ptr<IWireTransport> transport_;
    Worker service_worker_;
    TraceBuffer trace_;

    std::atomic<bool> running_{false};
    std::atomic<bool> transport_active_{false};
    std::atomic<bool> transport_connected_{false};
    std::atomic<std::uint64_t> dropped_messages_{0};

    mutable std::mutex transport_mutex_;
    EventHandler event_handler_;
};

} // namespace ar61850::dms
