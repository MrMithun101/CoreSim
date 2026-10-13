# Parallel physics

Milestone 12 parallelizes independent rigid-body integration through the Milestone 11 worker pool. Broad phase, narrow phase and the impulse solver remain serial. The caller waits for every integration range before collision detection, profiling or rendering can read its results. No GPU or angular dynamics work is included.

## API and ownership

`PhysicsSystem::step(world, dt, stats, profiler, &jobs, grain)` borrows an existing pool synchronously. Omitting the pool preserves serial execution. A `PhysicsSystem` instance permits one step at a time because it retains collision scratch storage; different instances may use the same pool with separate worlds. `integrate_bodies` also accepts the pool and grain for isolated measurements. Lookup and dense traversal use the same arithmetic; parallelism changes only which thread visits each body.

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

## Measured results — October 12, 2026

Machine: Apple M1, four performance and four efficiency cores (eight logical cores), 8 GiB RAM, macOS 26.3.1 (25D771280a), AppleClang 21.0.0.21000101, C++20 Release. No concurrent CoreSim builds/tests ran during measurements. Normal desktop services remained active; CPU placement, clock speed and background activity were uncontrolled. No GPU computation is involved.

The computational implementation and benchmark binary are from `d3ce3dd`. Raw data, commands, binary SHA-256, machine metadata and summaries are preserved in [main scaling](../benchmarks/results/parallel-m12/), [grain study](../benchmarks/results/parallel-grain-m12/), and [above-threshold collisions](../benchmarks/results/parallel-collisions-m12/). Together these contain 21,250 validated measured rows in 425 invocations; nothing was trimmed. Each configuration has five independent invocations and 250 measured ticks. The original grain run at 100,000 bodies is also retained, alongside its repeat in the expanded grain study. The final runner includes all scenarios; the two study switches reproduce the supplemental groups. The [pre-change serial baseline](../benchmarks/results/parallel-baseline-m12/) retains another 60 rows at `a855ddb`.

### Worker scaling

Median milliseconds per tick, default grain 16,384. Worker zero is the inline serial reference. **All rows below 16,384 bodies execute inline for every worker setting**; differences in those rows are measurement variation, not thread scaling. At 100,000 bodies, an eight-worker pool gets six tasks; at one million it gets eight.

| Workload | Bodies | Transform order | Serial | 1 worker | 2 workers | 4 workers | 8 workers |
|---|---:|---|---:|---:|---:|---:|---:|
| free-flight | 1,000 | ordered | 0.011 | 0.011 | 0.012 | 0.012 | 0.011 |
| free-flight | 1,000 | shuffled | 0.010 | 0.011 | 0.010 | 0.011 | 0.011 |
| free-flight | 10,000 | ordered | 0.097 | 0.098 | 0.097 | 0.097 | 0.097 |
| free-flight | 10,000 | shuffled | 0.099 | 0.101 | 0.100 | 0.101 | 0.099 |
| free-flight | 100,000 | ordered | 1.022 | 1.055 | 0.605 | 0.363 | 0.402 |
| free-flight | 100,000 | shuffled | 1.652 | 2.499 | 0.951 | 0.915 | 0.558 |
| free-flight | 1,000,000 | ordered | 10.444 | 11.367 | 5.799 | 6.122 | 5.697 |
| free-flight | 1,000,000 | shuffled | 38.483 | 38.490 | 23.077 | 14.066 | 13.655 |
| integration | 1,000,000 | ordered | 12.024 | 10.640 | 6.542 | 7.225 | 6.461 |
| collisions | 10,000 | ordered | 3.519 | 2.876 | 3.027 | 3.188 | 3.509 |
| collisions | 10,000 | shuffled | 2.729 | 2.771 | 2.733 | 2.742 | 2.735 |
| collisions | 100,000 | ordered | 32.602 | 33.105 | 32.183 | 32.171 | 32.215 |
| collisions | 100,000 | shuffled | 43.846 | 43.879 | 43.429 | 42.726 | 43.208 |

The 100,000-body ordered free-flight case improves from 1.022 to 0.363 ms with four workers (2.82×). At one million shuffled bodies, eight workers improve 38.483 to 13.655 ms (2.82×); four workers already reach 14.066 ms. These gains include submission and completion costs and the complete free-flight physics step. They do not apply to general collision scenes or rendered frames.

There is substantial variation in some workloads: the five trial medians for 100,000 ordered free-flight bodies range from 1.009–1.278 ms serial and 0.332–1.763 ms with four workers. One trial therefore exceeds the serial range despite the pooled median improvement. In contrast, the million-body shuffled case ranges from 38.178–38.771 ms serial and 13.449–13.971 ms with eight workers. All trial medians and raw min/max values remain in the JSON summaries; these are descriptive statistics, not confidence intervals. The isolated integration run occurred later than the free-flight run, so differences between their absolute times should not be interpreted as collision overhead or a different integration algorithm.

### Task granularity

Four-worker pool, ordered free-flight bodies, median milliseconds per complete tick. Grain is a minimum range size and an inline threshold; it does **not** request one job per grain-sized block. Tasks are capped at the worker count.

| Bodies | Grain 1 | Grain 1,024 | Grain 16,384 | Grain 65,536 | Grain 1,000,000 |
|---:|---:|---:|---:|---:|---:|
| 1,000 | 0.013167 | 0.011062 | 0.010916 | 0.010916 | 0.011083 |
| 10,000 | 0.035916 | 0.036333 | 0.099542 | 0.098209 | 0.099333 |
| 100,000 | 0.333209 | 0.326583 | 0.330021 | 1.038854 | 1.026500 |

Forcing four tasks at 1,000 bodies costs more than inline execution. At 10,000 bodies, smaller grains can win materially; the conservative default deliberately leaves that measured gain available through explicit configuration. At 100,000 bodies, grains 1, 1,024 and 16,384 all produce four tasks and similar results. Grain 65,536 produces just one task and loses the parallel benefit; grain 1,000,000 executes inline. The repeat at 100,000 bodies is consistent with the earlier grain sweep (four tasks around 0.33 ms, one task/inline around 1.03 ms). The default avoids dispatch for the demo and small scenes without trying to tune a universal crossover from one desktop machine.

### Diminishing returns and limits

- **Serial fraction:** At 100,000 ordered collision bodies, the total changes only from 32.602 ms serial to 32.171 ms with four workers. The serial reference spends 31.580 ms (96.9% of its tick) in collision processing, limiting an integration-only optimization to about 1.032× even if integration became free. Collision processing remains serial. The shuffled case changes from 43.846 to 42.726 ms. These small differences are not a broad collision-performance claim; speeding up integration cannot remove serial hash/query/solver costs.
- **Synchronization and contention:** Each parallel tick pays for a group allocation, queue locking/notifications, worker wakeups and a join. The small-body experiment exposes this overhead. The body loop itself takes no locks, and queue traffic is capped at the configured worker count. Contention was not independently profiled.
- **Cache locality:** Shuffling transform storage increases the million-body serial time from 10.444 to 38.483 ms, consistent with costly irregular joins. The storage layout has not changed, and no per-tick packing cost is hidden. Shared-cache behavior and cache misses were not measured with hardware counters.
- **Bandwidth and core differences:** Ordered million-body work levels off around 5.7–6.1 ms for 2/4/8 workers; eight workers are not an eightfold throughput increase. Shared memory traffic, cache capacity, scheduler placement, and the M1's four performance/four efficiency cores are plausible limits. These measurements do not isolate their individual contributions or prove bandwidth saturation.
- **False sharing:** Contiguous body ranges limit adjacent body writes to range boundaries. Shuffled transform entries can place writes from different workers on the same cache line. There is no per-body lock or shared writable progress counter; no padding was added without evidence. False-sharing counters were not collected, so it remains a possible cost, not an established explanation.
- **Task balance:** A single range per worker is simple and bounded, but cannot dynamically rebalance a slow core or an unusually expensive range. Missing/static-body distribution and heterogeneous cores can alter finish times. Smaller chunks/work stealing are future experiments, not part of this milestone.

Retain the serial reference, configurable grain and existing component layout. The application supplies its persistent pool, while its small demo stays inline. Keep the collision pipeline serial to preserve exact ordering; no angular dynamics or additional physics parallelism is introduced here.

Milestone 13 note: angular integration now uses the same disjoint ranges and completion barrier. The historical timings above predate that added work. Current benchmark CSV is schema 2, whose checksum also includes angular velocity and orientation.
