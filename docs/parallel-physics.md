# Parallel physics

Milestone 12 parallelizes independent rigid-body integration through the Milestone 11 worker pool. Broad phase, narrow phase and the impulse solver remain serial. The caller waits for every integration range before collision detection, profiling or rendering can read its results. No GPU or angular dynamics work is included.

## API and ownership

`PhysicsSystem::step(world, dt, stats, profiler, &jobs, grain)` borrows an existing pool synchronously. Omitting the pool preserves serial execution. `integrate_bodies` also accepts the pool and grain for isolated measurements. Lookup and dense traversal use the same arithmetic; parallelism changes only which thread visits each body.

World remains the exclusive structural owner. While a step executes, no caller may create/destroy entities, set/remove components, change forces, or read/write values being integrated. Each worker owns a disjoint contiguous range of rigid-body entries. An entity has at most one body and transform, so those ranges also own distinct transform writes. Sparse indices, entity generations, collider values and container metadata are read-only. Shuffled transforms affect locality, not ownership. `for_each_rigid_body_range` exposes mutable body values with const entity IDs and checks range bounds.

The default grain is 16,384 bodies. Below it, integration runs inline. Otherwise the task count is `min(body_count / grain, worker_count)`, and balanced contiguous ranges differ by at most one body. There is at most one queued task per worker, with no per-body job or temporary gather/scatter buffer. A fresh completion group and a small number of queue entries are allocated per parallel tick. One-worker mode really dispatches through the pool when above the threshold; it is distinct from the serial baseline. Grain is configurable for measurement and is not an optimal value for every machine.

The coordinator performs all waits and profiler updates. Any job-system worker attempting synchronous integration is rejected before submission, including below the serial threshold. This prevents nested waits and prevents early unwinding from leaving jobs borrowing stack state. If submission throws after accepting earlier jobs, those jobs are drained before the original admission error is rethrown. If a range throws, all sibling tasks finish before the exception reaches the caller. As with serial integration, failed steps can partially update the world; there is no rollback, and collision solving does not run. Multiple simultaneous errors need not select the same exception or updated subset across schedules. Valid inputs preserve exact per-body arithmetic and serial contact order.

## Verification

Dedicated concurrency tests compare every velocity, position and force exactly across 1/2/4/8 workers, both traversal modes, shuffled joins, missing transforms, static bodies, non-divisible ranges and structural churn between ticks. Full physics comparisons exercise both broad phases, mixed collision primitives and a static plane. Other cases cover invalid inputs, throwing ranges, closed/racing shutdown, empty/small worlds, const entity identities, invalid ranges and nested worker rejection. CI repeats both the job and physics suites ten times under ThreadSanitizer.

## Reproduce scaling

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
./build-release/coresim_parallel_benchmark --workload free-flight --bodies 100000 --workers 4
python3 benchmarks/compare_parallel.py ./build-release/coresim_parallel_benchmark benchmarks/results/parallel-local
```

The executable accepts `--workers 0..8` (zero is serial), `--grain`, `--bodies`, `--steps`, `--warmup`, and `--order ordered|shuffled`. Its workloads are isolated `integration`, full `free-flight` physics without colliders, and full `collisions` physics using the sparse mixed sphere/box world from Milestone 10. All include 10% static bodies, missing transforms at every seventeenth entity, varied masses/velocities, persistent acceleration and initial forces. Initial forces are consumed during warmup; steady-state measurements still include force reads and clears. These are controlled workloads, not a claim about general engine frame rate or dense stacks.

CSV timing includes submission, wakeups, integration and waits; full physics additionally includes collision work. Pool construction, world setup, CSV output and checksum reduction are excluded. Collision timing is reported separately. The runner rotates all five configurations over five trials, retains every raw tick, checks exact checksum/collision-counter equality between configurations, and saves median/min/max plus individual trial medians. A checksum is not a proof of state equality; the automated tests compare individual values. Warmup advances the world rather than resetting it. No affinity or real-time scheduling is imposed.

Measured results and interpretation will be recorded after the full scaling runs.
