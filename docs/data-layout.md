# Data-oriented performance investigation

Milestone 10 keeps the existing component layout and **lookup traversal as the production default**. It adds a measured, opt-in direct dense traversal candidate: the reference walks packed body entries and then looks up each body through its entity ID, generation slot, and sparse index; the candidate receives the already available mutable body value and retains the existing transform/collider joins. Entity IDs cannot be edited through the visitor. Structural mutations during traversal remain prohibited, just as for the existing component spans.

The measured alternatives include packed AoS and scalar SoA work buffers. Neither justified changing production ownership/storage after gathering and writeback were included. Dense traversal improved isolated integration but did not consistently improve full physics ticks in two recorded runs, so it was not promoted to the final default. Rendering, collider geometry, collision ordering, mass semantics, force clearing, and exception validation remain unchanged. `PhysicsSystem` defaults to `BodyIteration::lookup`; `BodyIteration::dense` remains explicitly selectable for further investigation.

## Existing storage and experimental layouts

`ComponentPool<T>` already owns a contiguous vector of `{Entity, T}` entries and a sparse index. Different pools have independent insertion/removal orders. Transform also contains rotation/scale, while integration needs only position. RigidBody contains velocity, acceleration, force, mass/inverse mass, and restitution. The reference traversal touches generation/sparse lookup data for the body, transform, and collider; direct traversal removes only the redundant body lookup.

Measured local ABI sizes (Apple Clang 21, arm64): Entity 16 bytes, RigidBody 48, body entry 64, Transform 40, transform entry 56. The experimental packed AoS row has position/velocity/acceleration/force vec3 values and inverse mass: **52 bytes**. The SoA has 13 independent `vector<float>` columns for those same scalars, also **52 bytes of numeric payload per body**, plus vector headers/capacity. Both experiments additionally retain two writeback pointers per body (16 bytes on this host) and coexist with the canonical World storage. They therefore increase memory traffic/storage rather than replacing it. No packed buffer is used in the production path.

| Mode | Work timed |
| --- | --- |
| `lookup` | Original per-body lookup + transform/collider joins + integration/validation + force clearing |
| `dense` | Direct body-value traversal + the same joins, arithmetic, validation, and clearing |
| `aos` | Gather hot fields and targets, integrate contiguous rows, scatter results/clear forces |
| `soa` | Gather 13 scalar columns and targets, integrate columns, scatter results/clear forces |
| `physics-lookup` / `physics-dense` | Entire production physics step using the chosen integration traversal plus the same spatial hash, narrow phase, and solver |

Packed buffers retain capacity and regather pointers/fields every tick; no persistent pointer cache survives structural edits. Dynamic plane validation happens during gather and nonfinite output validation during the packed kernel. Those experimental modes can leave different partial state on an exception, so they are benchmarks, not interchangeable production error-recovery policies. No explicit SIMD, fast-math, aliasing annotations, or threading was added. These results evaluate these implementations and conversion costs; they do not show that a persistent SoA owner or vectorized SoA kernel could never be useful.

## Reproduction and workload

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCORESIM_BUILD_APP=OFF
cmake --build build-release --parallel
./build-release/coresim_layout_benchmark --layout dense --bodies 100000 --steps 100 --warmup 10 --order shuffled > dense.csv
python3 benchmarks/compare_layouts.py build-release/coresim_layout_benchmark benchmarks/results/my-layout-run
python3 benchmarks/compare_layouts.py build-release/coresim_layout_benchmark benchmarks/results/my-physics-run --physics
```

Choose a new result directory each time. The tool is always available, including headless builds. CLI bounds: bodies 1–1,000,000, steps 1–10,000, warmup 0–10,000; defaults are 10,000, 100, and 10. Layout selection is required; default order is `ordered`. Invalid, duplicate, or missing options fail with a nonzero exit code.

Bodies have alternating sphere/box colliders, varied masses and initial velocities, persistent user acceleration, gravity, and an initial force. Every tenth body is static and every seventeenth lacks a transform, exercising both integration branches and force clearing. Transform insertion is either ascending entity order or deterministically shuffled using seeded MT19937 and explicit Fisher-Yates. Initial positions form a spaced lattice. This represents moving mixed components and disrupted pool order; it is synthetic, not a captured game trace or a dense-contact benchmark. Full-physics modes use the same world and run collision detection too. Contact/stack correctness is tested separately.

The timestep is 1/120 second. Each invocation starts a fresh world and executes 10 warmup ticks plus 100 recorded ticks. Forces are cleared on the first warmup tick; later ticks still have gravity and persistent acceleration. Setup, warmup, checksum evaluation, CSV formatting and I/O are excluded from wall timings. Every recorded tick computes a position/velocity checksum to consume and validate the updated state; that pass can warm component caches before the next tick in every mode. Measurements are steady-state, not cold-cache or allocation-heavy workloads.

The runner makes four sequential trials per mode/workload, rotating order so each of four integration modes occupies every position once. Full-physics trials alternate the two modes. It records 400 ticks per configuration, validates per-tick state checksums against the reference within tolerance, retains all samples without outlier removal, and reports medians/min/max. Integration trials use 10,000 and 100,000 bodies; full physics uses 1,000 and 10,000. Timings use `steady_clock` in milliseconds. `gather_ms`, `kernel_ms`, `scatter_ms`, and `total_ms` distinguish conversion from numeric work. For lookup/dense and full-physics modes there is no packing; kernel and total cover the whole selected operation. A physics-mode result is a full physics tick, not a numeric kernel.

Build configuration/compiler, layout/order, body count, tick, warmup count, durations, and checksum are in each CSV row. Machine/revision metadata and ordered command lists accompany saved runs. Compare same builds, machines, and workloads; no unit test asserts a speed threshold.

## Hardware-counter follow-up on Linux

This measurement host is macOS; Linux `perf` is not installed here. CPU cycles, instructions, cache misses, and branch misses were **not measured**, and no miss-rate or IPC improvement is claimed. Wall times alone do not establish which cache or branch mechanism caused a difference. The shuffled workload's slowdown is consistent with poorer access locality, but it is not a hardware-counter diagnosis.

On a Linux host with permitted PMU access, first use `perf list` to see supported events, then collect both reference and candidate with matching settings:

```sh
perf stat -r 5 -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
  -o lookup-perf.txt -- ./build-release/coresim_layout_benchmark \
  --layout lookup --bodies 100000 --steps 1000 --warmup 20 --order shuffled > /dev/null
perf stat -r 5 -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
  -o dense-perf.txt -- ./build-release/coresim_layout_benchmark \
  --layout dense --bodies 100000 --steps 1000 --warmup 20 --order shuffled > /dev/null
```

Record CPU/kernel/compiler/build and event support. Inspect cycles, instructions, IPC, cache-miss/reference ratio and branch-miss/branch ratio where corresponding events are available. These process-wide counts include setup, warmup, checksums, and CSV formatting, unlike the scoped wall timings; do not label them kernel-only counts. Check multiplexing/event running percentages and retain unsupported or permission errors as unavailable data. Do not assume generic cache events describe a particular cache level. The command flags and event-selection workflow follow the [upstream perf-stat manual](https://man7.org/linux/man-pages/man1/perf-stat.1.html).

## Recorded measurements

Local Release measurements and the resulting decision are recorded below with the raw data links.

### Integration results — October 9, 2026

Measured on Apple M1, 8 logical CPU cores, 8 GiB unified RAM, macOS 26.3.1(a), Apple Clang 21.0.0, arm64 Release `-O3 -DNDEBUG`, CMake 3.31.6, no sanitizers. GPU unused; background load, affinity and thermal state uncontrolled. Integration dataset revision: `56eda63`. Full-physics datasets: `ccac0f4`. The final lookup-default decision does not change the explicit benchmark modes. All runs finished before the final test/build jobs began.

Median total integration milliseconds over 400 recorded ticks per cell:

| Bodies | Transform order | Lookup | Dense candidate | Packed AoS | Packed SoA |
| ---: | --- | ---: | ---: | ---: | ---: |
| 10,000 | ordered | 0.099875 | 0.076916 | 0.140333 | 0.477500 |
| 10,000 | shuffled | 0.101042 | 0.074146 | 0.140937 | 0.476417 |
| 100,000 | ordered | 1.043979 | 0.814292 | 1.645875 | 4.749000 |
| 100,000 | shuffled | 1.889562 | 1.354834 | 2.617104 | 5.153750 |

Dense traversal reduced isolated integration median time by 22.0–28.3% in this run. Both packed variants lost on total cost. At 100,000 shuffled bodies, packed AoS spent 1.844 ms gathering, 0.287 ms in its numeric kernel, and 0.481 ms scattering (2.617 ms median total), versus 1.890 ms for lookup. SoA spent 3.986 ms gathering, 0.560 ms in its kernel, and 0.571 ms scattering (5.154 ms total). Component medians need not sum to the median total. A kernel-only comparison would hide most of the cost and suggest the wrong production decision.

### Full-physics cross-check and repeat

Median full physics tick milliseconds, including the same spatial hash, detection and solver. Positive change means dense was slower. Each dataset contains four balanced trials with 400 ticks per configuration; the repeat is reported separately rather than substituted for the first run.

| Run | Bodies | Order | Lookup (ms) | Dense (ms) | Dense time change |
| --- | ---: | --- | ---: | ---: | ---: |
| Initial | 1,000 | ordered | 0.289604 | 0.270813 | -6.49% |
| Initial | 1,000 | shuffled | 0.269875 | 0.271250 | +0.51% |
| Initial | 10,000 | ordered | 2.673750 | 2.873708 | +7.48% |
| Initial | 10,000 | shuffled | 2.746688 | 3.097354 | +12.77% |
| Repeat | 1,000 | ordered | 0.265896 | 0.268521 | +0.99% |
| Repeat | 1,000 | shuffled | 0.274709 | 0.275791 | +0.39% |
| Repeat | 10,000 | ordered | 2.803834 | 2.915083 | +3.97% |
| Repeat | 10,000 | shuffled | 2.872084 | 2.803229 | -2.40% |

The initial full-step run did not reproduce the isolated integration win, with the larger dense cases slower. A second complete comparison still showed no consistent improvement across workloads. There were substantial timing tails in the ordinary desktop session; all are preserved in the CSV and min/max summaries. These runs are insufficient to attribute the differences to a specific cache mechanism or to assert a statistically significant regression. They are also insufficient to justify changing the engine's default on the basis of the microbenchmark alone.

**Final decision:** retain the original component layout and lookup integration default. Keep direct dense traversal as an opt-in candidate and keep both packed layouts in the experimental benchmark only. A future promotion should require a stable end-to-end benefit on representative scenes, preferably with CPU-counter evidence. The negative result and the repeat are part of the milestone deliverable; no full-engine speedup is claimed.

All 12,800 measured rows passed workload and state-checksum validation. Tests additionally compare per-body positions/velocities through storage churn, missing components, static bodies, invalid inputs, and real collision response. The packed alternatives are not generalized to rendering or other components.

Raw data, metadata, ordered commands and full median/min/max summaries:

- [Integration trials](../benchmarks/results/m10-m1/) · [metadata](../benchmarks/results/m10-m1/metadata.json) · [summary](../benchmarks/results/m10-m1/summary.json)
- [Initial full physics trials](../benchmarks/results/m10-physics-m1/) · [metadata](../benchmarks/results/m10-physics-m1/metadata.json) · [summary](../benchmarks/results/m10-physics-m1/summary.json)
- [Repeated full physics trials](../benchmarks/results/m10-physics-repeat-m1/) · [metadata](../benchmarks/results/m10-physics-repeat-m1/metadata.json) · [summary](../benchmarks/results/m10-physics-repeat-m1/summary.json)
