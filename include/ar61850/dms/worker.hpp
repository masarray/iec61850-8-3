#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace ar61850::dms {

class Worker {
public:
    using Task = std::function<void()>;

    explicit Worker(std::size_t max_pending = 4096);
    ~Worker();

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    bool post(Task task);
    void stop() noexcept;
    std::size_t pending() const noexcept;

private:
    void run(std::stop_token token);

    const std::size_t max_pending_;
    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::queue<Task> queue_;
    std::jthread thread_;
    bool stopping_{false};
};

} // namespace ar61850::dms
