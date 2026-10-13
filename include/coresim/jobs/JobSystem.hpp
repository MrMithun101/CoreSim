#pragma once
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace coresim {
using Job = std::function<void()>;
class JobSystem;
// Copyable completion handle. First wait seals the group against further submissions.
class JobGroup {
public:
    JobGroup();
    void wait() const;
private:
    struct State;
    std::shared_ptr<State> state_;
    friend class JobSystem;
};
class JobSystem {
public:
    // Zero chooses hardware_concurrency (at least one). Threads are created only here.
    explicit JobSystem(std::size_t workers = 0);
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    void submit(const JobGroup& group, Job job);
    [[nodiscard]] JobGroup submit(Job job);
    // Stop admission, drain accepted jobs, and join. Concurrent calls are serialized.
    void shutdown();
    // Synchronous clients must reject nested waits before submitting borrowed work.
    [[nodiscard]] static bool is_worker_thread() noexcept;
    [[nodiscard]] std::size_t worker_count() const noexcept { return workers_.size(); }
private:
    struct WorkItem { Job job; std::shared_ptr<JobGroup::State> group; };
    void worker_loop();
    std::mutex queue_mutex_;
    std::condition_variable ready_;
    std::deque<WorkItem> queue_;
    bool closing_{};
    std::mutex shutdown_mutex_;
    std::vector<std::thread> workers_;
};
} // namespace coresim
