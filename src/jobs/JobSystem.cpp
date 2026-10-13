#include <coresim/jobs/JobSystem.hpp>
#include <algorithm>
#include <exception>
#include <limits>
#include <stdexcept>

namespace coresim {
namespace {
// Reject blocking pool/group operations on any pool worker: no hidden starvation cycles.
thread_local bool on_worker = false;
void require_external_thread() {
    if (on_worker) { throw std::logic_error("Workers may submit jobs but cannot wait or shut down pools"); }
}
}
struct JobGroup::State {
    std::mutex mutex;
    std::condition_variable completed;
    std::size_t pending{};
    bool sealed{};
    std::exception_ptr failure;
};
JobGroup::JobGroup() : state_(std::make_shared<State>()) {}
void JobGroup::wait() const {
    require_external_thread();
    if (!state_) { throw std::invalid_argument("Moved-from job group"); }
    std::exception_ptr failure;
    {
        std::unique_lock lock(state_->mutex);
        state_->sealed = true;
        state_->completed.wait(lock, [&] { return state_->pending == 0; });
        failure = state_->failure;
    }
    if (failure) { std::rethrow_exception(failure); }
}
JobSystem::JobSystem(std::size_t workers) {
    require_external_thread();
    if (workers == 0) { workers = std::max(1U, std::thread::hardware_concurrency()); }
    workers_.reserve(workers);
    try {
        for (std::size_t i = 0; i < workers; ++i) { workers_.emplace_back([this] { worker_loop(); }); }
    } catch (...) {
        shutdown(); // Wake/join partially constructed workers before their thread objects unwind.
        throw;
    }
}
JobSystem::~JobSystem() { shutdown(); }
void JobSystem::submit(const JobGroup& group, Job job) {
    if (!job || !group.state_) { throw std::invalid_argument("Submission needs a job and valid group"); }
    {
        // Global lock order: group -> queue. Workers never hold queue while completing a group.
        std::lock_guard group_lock(group.state_->mutex);
        if (group.state_->sealed) { throw std::logic_error("Cannot submit to a sealed job group"); }
        if (group.state_->pending == std::numeric_limits<std::size_t>::max()) {
            throw std::overflow_error("Job group pending count overflow");
        }
        std::lock_guard queue_lock(queue_mutex_);
        if (closing_) { throw std::runtime_error("Job system is shut down"); }
        queue_.push_back({std::move(job), group.state_});
        // Publish the count only after allocating queue storage succeeds, before workers can pop.
        ++group.state_->pending;
    }
    ready_.notify_one();
}
JobGroup JobSystem::submit(Job job) {
    JobGroup group;
    submit(group, std::move(job));
    return group;
}
bool JobSystem::is_worker_thread() noexcept { return on_worker; }
void JobSystem::shutdown() {
    require_external_thread();
    std::lock_guard shutdown_lock(shutdown_mutex_);
    {
        std::lock_guard queue_lock(queue_mutex_);
        closing_ = true;
    }
    ready_.notify_all();
    for (auto& worker : workers_) { if (worker.joinable()) { worker.join(); } }
}
void JobSystem::worker_loop() {
    on_worker = true;
    for (;;) {
        WorkItem item;
        {
            std::unique_lock lock(queue_mutex_);
            ready_.wait(lock, [this] { return closing_ || !queue_.empty(); });
            if (queue_.empty()) { break; }
            item = std::move(queue_.front());
            queue_.pop_front();
        }
        std::exception_ptr failure;
        try { item.job(); } catch (...) { failure = std::current_exception(); }
        item.job = {}; // Release captures before publishing group completion.
        bool finished;
        {
            std::lock_guard lock(item.group->mutex);
            if (failure && !item.group->failure) { item.group->failure = failure; }
            finished = --item.group->pending == 0;
        }
        if (finished) { item.group->completed.notify_all(); }
    }
    on_worker = false;
}
} // namespace coresim
