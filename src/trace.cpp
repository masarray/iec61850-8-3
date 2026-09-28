#include "ar61850/dms/trace.hpp"

#include <algorithm>
#include <utility>

namespace ar61850::dms {

TraceBuffer::TraceBuffer(std::size_t capacity)
    : capacity_(std::max<std::size_t>(1, capacity)) {
    events_.reserve(capacity_);
}

void TraceBuffer::push(TraceEvent event) {
    std::scoped_lock lock(mutex_);
    event.sequence = next_sequence_++;
    if (events_.size() == capacity_) events_.erase(events_.begin());
    events_.push_back(std::move(event));
}

std::vector<TraceEvent> TraceBuffer::snapshot() const {
    std::scoped_lock lock(mutex_);
    return events_;
}

std::size_t TraceBuffer::size() const noexcept {
    std::scoped_lock lock(mutex_);
    return events_.size();
}

void TraceBuffer::clear() {
    std::scoped_lock lock(mutex_);
    events_.clear();
}

} // namespace ar61850::dms
