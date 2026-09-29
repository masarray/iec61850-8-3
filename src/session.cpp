#include "ar61850/dms/session.hpp"

#include <utility>

namespace ar61850::dms {

Session::Session(std::size_t max_outstanding) : max_outstanding_(max_outstanding) {}

std::uint32_t Session::next_invoke_id() noexcept {
    return invoke_id_.fetch_add(1, std::memory_order_relaxed);
}

bool Session::register_request(std::uint32_t invoke_id, ServiceKind service) {
    std::scoped_lock lock(mutex_);
    if (pending_.size() >= max_outstanding_) return false;
    return pending_.emplace(
        invoke_id,
        PendingRequest{service, std::chrono::steady_clock::now()}
    ).second;
}

std::optional<PendingRequest> Session::complete_request(std::uint32_t invoke_id) {
    std::scoped_lock lock(mutex_);
    const auto it = pending_.find(invoke_id);
    if (it == pending_.end()) return std::nullopt;
    auto result = it->second;
    pending_.erase(it);
    return result;
}

std::size_t Session::outstanding() const noexcept {
    std::scoped_lock lock(mutex_);
    return pending_.size();
}

void Session::set_associate_id(std::string id) {
    std::scoped_lock lock(mutex_);
    associate_id_ = std::move(id);
}

std::string Session::associate_id() const {
    std::scoped_lock lock(mutex_);
    return associate_id_;
}

void Session::set_state(AssociationState state) noexcept {
    state_.store(state, std::memory_order_release);
}

AssociationState Session::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

} // namespace ar61850::dms
