#pragma once

#include "ar61850/dms/service.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ar61850::dms {

enum class Direction : std::uint8_t { Rx, Tx };
enum class EndpointRole : std::uint8_t { Client, Server };

struct TraceEvent {
    std::uint64_t sequence{0};
    std::chrono::system_clock::time_point observed_at{};
    EndpointRole endpoint{EndpointRole::Client};
    Direction direction{Direction::Rx};
    MessageClass message_class{MessageClass::Request};
    ServiceKind service{ServiceKind::Unknown};
    std::size_t byte_count{0};
    std::optional<std::uint32_t> invoke_id;
    std::string associate_id;
};

class TraceBuffer {
public:
    explicit TraceBuffer(std::size_t capacity = 4096);
    void push(TraceEvent event);
    std::vector<TraceEvent> snapshot() const;
    std::size_t size() const noexcept;
    void clear();

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::vector<TraceEvent> events_;
    std::uint64_t next_sequence_{1};
};

} // namespace ar61850::dms
