#include "ar61850/dms/worker.hpp"

#include <utility>

namespace ar61850::dms {

Worker::Worker(std::size_t max_pending)
    : max_pending_(max_pending), thread_([this](std::stop_token token) { run(token); }) {}

Worker::~Worker() { stop(); }

bool Worker::post(Task task) {
    std::scoped_lock lock(mutex_);
    if (stopping_ || queue_.size() >= max_pending_) return false;
    queue_.push(std::move(task));
    cv_.notify_one();
    return true;
}

void Worker::stop() noexcept {
    {
        std::scoped_lock lock(mutex_);
        if (stopping_) return;
        stopping_ = true;
    }
    thread_.request_stop();
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

std::size_t Worker::pending() const noexcept {
    std::scoped_lock lock(mutex_);
    return queue_.size();
}

void Worker::run(std::stop_token token) {
    while (!token.stop_requested()) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, token, [this] { return stopping_ || !queue_.empty(); });
            if ((stopping_ || token.stop_requested()) && queue_.empty()) break;
            if (queue_.empty()) continue;
            task = std::move(queue_.front());
            queue_.pop();
        }
        if (task) task();
    }
}

} // namespace ar61850::dms
