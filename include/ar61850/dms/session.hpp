#pragma once

#include "ar61850/dms/service.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace ar61850::dms {

enum class AssociationState : std::uint8_t {
    Disconnected,
    Connecting,
    Associated,
    Releasing,
    Failed
};

struct PendingRequest {
    ServiceKind service{ServiceKind::Unknown};
    std::chrono::steady_clock::time_point created_at{};
};

class Session {
public:
    explicit Session(std::size_t max_outstanding = 64);

    std::uint32_t next_invoke_id() noexcept;
    bool register_request(std::uint32_t invoke_id, ServiceKind service);
    std::optional<PendingRequest> complete_request(std::uint32_t invoke_id);
    std::size_t outstanding() const noexcept;

    void set_associate_id(std::string id);
    std::string associate_id() const;
    void set_state(AssociationState state) noexcept;
    AssociationState state() const noexcept;

private:
    const std::size_t max_outstanding_;
    std::atomic<std::uint32_t> invoke_id_{0};
    std::atomic<AssociationState> state_{AssociationState::Disconnected};
    mutable std::mutex mutex_;
    std::string associate_id_;
    std::unordered_map<std::uint32_t, PendingRequest> pending_;
};

} // namespace ar61850::dms
