# Fixed worker-pool job system

Milestone 11 adds `coresim_jobs`, a C++20 library using a fixed vector of `std::thread` workers, a mutex-protected FIFO `deque`, and a condition variable. Workers are created only during pool construction. Idle workers block on the queue predicate (`closing || !queue.empty()`), rather than spinning. Each notification wakes a worker; shutdown wakes all. Execution and completion order are unspecified even though dequeue order is FIFO. No lock-free structures or work stealing are used.

```cpp
#include <coresim/jobs/JobSystem.hpp>

std::vector<int> output(10000); // Outlives the pool, including if submission throws.
coresim::JobSystem jobs(4);
coresim::JobGroup batch;
for (std::size_t i = 0; i < output.size(); ++i) {
    jobs.submit(batch, [&, i] { output[i] = static_cast<int>(i * i); });
}
batch.wait(); // Seals the batch, waits for all jobs, and publishes their writes.
// output is now safe to read on this thread.
jobs.submit([] { /* one independent task */ }).wait();
```

Zero workers in the constructor selects `hardware_concurrency()`, falling back to one if unavailable. Explicit positive counts are honored; `worker_count()` reports that configured size, including after shutdown. Pool creation and joining belong to an external coordinating thread. The application owns a default pool as its last member, so its destructor joins workers before scene/graphics resources are destroyed. This milestone intentionally leaves physics, rendering, GLFW, and the current profiler on the main thread; parallel physics is Milestone 12.

## Completion and failures

`Job` is `std::function<void()>`: callbacks must be copy-constructible (shared ownership can wrap move-only state). `submit(job)` returns a completion group for one job; `submit(group, job)` appends to a caller-supplied group. The queue is unbounded and each pending item owns its callable and a shared group state. Captured references must remain valid until completion; callers are responsible for avoiding data races inside their jobs.

A `JobGroup` is a copyable shared completion handle. Its first `wait()` seals admission under the same mutex used by submission, then waits until its pending count reaches zero. A racing submit either becomes part of the group before sealing, or fails without enqueueing. For batches built by multiple producer threads, join those producers before waiting unless rejection is intended. A group is single-use: create a fresh group for the next batch. Waiting again is valid and reports the same recorded exception, if any. Waiting on an empty group seals it immediately. Moving/assigning a handle while another thread uses that same handle object is not supported; use copies instead. Jobs from different pools may share a group, provided all pools and referenced data have suitable lifetimes.

Workers catch all callback exceptions, retain the first failure **observed at completion**, and continue processing siblings. Group wait rethrows that failure only after all accepted jobs finish. There is no cancellation or fail-fast stopping. A callable's captures are released before publishing completion. Destructors drain and join but do not throw job exceptions; callers must wait on groups to observe failures. Dropping all external group handles does not abandon queued work, because queued/running jobs retain the completion state. A group may outlive its pool. Moved-from groups and empty callables are rejected.

## Shutdown and deadlock policy

`shutdown()` closes admission atomically with queue access, drains all accepted jobs, wakes sleepers, and joins every worker. Later submissions fail. Concurrent shutdown callers are serialized; subsequent calls are harmless. Submission may race with explicit shutdown while the object remains alive: acceptance or rejection is determined under the queue lock. Destruction itself must not race with public calls; the owner must ensure the pool remains alive until those calls return. A thread-creation failure closes and joins any workers already created before unwinding.

Workers may submit additional jobs, but **cannot wait on groups, construct pools, or shut down pools**, including other pools. These calls throw `logic_error` so fixed-size pools cannot deadlock through nested worker waits or joins. Descendant submission still follows group sealing and pool-closing rules; use a separate child group and have the coordinating thread wait after the producer job completes. Destroying a pool on a worker violates its lifetime contract and terminates via the noexcept destructor. A callback that blocks forever or synchronously waits on a user-created dependency cycle prevents draining; there is no forced thread termination.

The lock order for submission is group mutex → queue mutex. Workers release the queue mutex before invoking user code and before completing a group. Shutdown holds no queue mutex while joining. No user callback executes under an internal lock. The count is published only after queue allocation succeeds, preventing failed submissions from leaving a permanently pending group. Group completion/wait synchronization establishes visibility of job writes to waiters.

## Verification

The dedicated `coresim.jobs` CTest has a 60-second watchdog and a `concurrency` label. It covers 12,000 exactly-once jobs with each of one/two/four workers, concurrent producers and waiters, exception propagation with draining siblings, producer/shutdown races, simultaneous shutdown callers, destruction with queued work, repeated pools/bursts, worker-side blocking rejection, descendant submission, captured-resource release, dropped/moved handles, wait/submit sealing races, group independence from unrelated blocked work, and non-standard exceptions. Worker callbacks never invoke Catch2 assertions; results are checked by the coordinating test thread. The burst and lifecycle tests exercise wakeups and idle shutdown; the blocking condition-variable implementation supplies the no-spin idle behavior, without fragile wall-time assertions.

```sh
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DCORESIM_BUILD_APP=OFF -DCORESIM_SANITIZER=thread
cmake --build build-tsan --target coresim_jobs_tests --parallel
ctest --test-dir build-tsan -R '^coresim.jobs$' --repeat until-fail:10 --output-on-failure
```

ThreadSanitizer is separate from ASan/UBSan. CI adds a macOS ThreadSanitizer job repeating the stress suite ten times, alongside Linux GCC/Clang Debug/Release and headless ASan/UBSan. Sanitizer success is evidence for exercised schedules, not a proof of race freedom. No throughput or parallel-physics speedup is claimed.
