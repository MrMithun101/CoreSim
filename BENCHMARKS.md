# Collision benchmarks

Milestone 7 measures the deliberately naive all-pairs implementation. No spatial pruning is performed. Run a **Release** build without sanitizers for performance measurements:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
./build-release/coresim --benchmark collision-naive --bodies 10000 --steps 10 --warmup 2 > naive-10000.csv
```

This path returns before creating a GLFW window. For a build with no GLFW/OpenGL dependency, configure `-DCORESIM_BUILD_APP=OFF` and run `coresim_benchmark` with identical arguments. Output goes to stdout; errors go to stderr with a nonzero exit code. Redirect stdout to preserve CSV. `--bodies` accepts 1–100000, `--steps` 1–10000, and `--warmup` 0–10000. Defaults are 1000, 10, and 2. Large values intentionally become very slow; these limits validate inputs, not promise a reasonable runtime. Duplicate, unknown, malformed, and mixed interactive/benchmark arguments are rejected.

## Workload and timing boundaries

The `separated-spheres` scene is deterministic: N dynamic unit-mass spheres of radius 1 are placed on a lattice with spacing 3. The side length is the smallest integer whose cube contains N, filling x, then y, then z. Gravity, velocities, accelerations, and restitution are zero. Every tick uses 1/120 second. No random seed is needed and positions remain constant across warmup and measured ticks. World creation, warmup, CSV formatting, and file I/O are excluded from measurements.

This deliberately empty-contact scene isolates the wasted work of testing spatially distant bodies. It does **not** characterize dense contacts, solver scaling, interactive rendering, or frame pacing. Candidate pairs and initial checks must both be N(N−1)/2 each tick, while contacts and correction checks must be zero. For 10000 bodies, that is **49,995,000 checks per tick** even with no collisions.

Candidate generation enumerates each i<j pair in component order into a fixed 4096-pair scratch batch. Narrow phase consumes each batch before enumeration resumes. This preserves O(N²) work and pair order with bounded candidate storage (64 KiB on the measured 64-bit host), instead of storing tens of millions of pairs. Contact storage remains proportional to detected contacts. The naive solver uses this enumeration; as of Milestone 8 the interactive solver defaults to spatial hashing. Timing is opt-in via `CollisionStats*`, with no clock reads when omitted.

The archived Milestone 7 CSV schema version 1 emits one row per measured tick, with no discarded outliers:

| Field | Meaning |
| --- | --- |
| schema, benchmark, scene | Format version and workload identifiers |
| build_type, compiler | CMake configuration and compiler ID/version |
| bodies, step, warmup, timestep_s | N, zero-based measured tick, warmup count, fixed timestep |
| candidate_pairs | All unordered collider pairs before eligibility filtering |
| collision_checks | Initial calls to `detect_contact`, after skipping absent transforms and static/static pairs; unsupported shape combinations still count |
| contacts | Contacts collected by the initial narrow phase |
| correction_checks | Additional contact tests in positional correction; separate from initial checks |
| broad_phase_ms | Sum of pair-enumeration/batch-fill durations |
| narrow_phase_ms | Sum of eligibility filtering, initial detection, and constraint construction durations |
| solver_ms | Eight velocity and four position iterations, including correction rechecks |
| physics_step_ms | Entire integration + collision solve measured with a steady clock |
| total_frame_ms | Entire headless tick enclosing physics; no renderer or presentation |

Timings use `std::chrono::steady_clock`, serialized in milliseconds to six decimal places (format precision is not clock accuracy). Phase timings include instrumentation overhead. Physics totals also include integration, setup, and gaps between phase measurements, so phase sums need not equal totals. Headless frame and physics times are intentionally almost identical. Compare only equivalent builds, workloads, instrumentation, and machines. Record machine/OS/compiler, revision, command, and background load alongside CSV; the executable does not gather hardware metadata automatically.

## Recorded measurements

Measured October 8, 2026 at revision `33fc96520cbc97fc0ca7fd3d9f8799681eca7bf8` on Apple M1 (8 logical CPU cores), 8 GiB unified RAM, macOS 26.3.1(a), build 25D771280a. The 8-core integrated M1 GPU is unused. Apple Clang 21.0.0, CMake 3.31.6, Release `-O3 -DNDEBUG`, arm64, no sanitizers. Runs were sequential in ascending body count, each with two warmup ticks and ten measured ticks; no outliers were removed. Ordinary desktop session, no controlled CPU affinity, thermal state, or background load. These short runs establish a local baseline, not statistically portable throughput claims.

| Bodies | Candidate pairs / checks per tick | Median broad phase (ms) | Median narrow phase (ms) | Median physics (ms) | Physics min–max (ms) | Median headless frame (ms) |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1,000 | 499,500 | 1.021 | 4.810 | 5.847 | 5.767–6.235 | 5.847 |
| 5,000 | 12,497,500 | 25.635 | 123.655 | 149.407 | 147.184–152.785 | 149.407 |
| 10,000 | 49,995,000 | 102.020 | 488.723 | 591.119 | 585.925–601.070 | 591.119 |

All 30 measured rows have the expected exact pair/check count and zero contacts/correction checks. Solver times are near clock resolution in this workload. Component medians need not sum to the median total.

A tenfold body increase required about 101 times the physics time. At 10,000 bodies a tick took roughly 0.59 seconds, far beyond the 8.33 ms budget for 120 Hz physics. Even separated bodies incur all-pairs checks. This is the poor-performance baseline that spatial hashing in Milestone 8 must be compared against using the same scene and machine; no speedup has been implemented here.

Raw rows: [1,000 bodies](benchmarks/results/naive-m1-1000.csv), [5,000 bodies](benchmarks/results/naive-m1-5000.csv), [10,000 bodies](benchmarks/results/naive-m1-10000.csv). [Machine and run metadata](benchmarks/results/naive-m1-metadata.json) records the exact commands. Each CSV includes all ten measured ticks. Repeat runs may differ.


## Milestone 8: spatial-hash comparison

The application now uses the [spatial hash](docs/spatial-hash.md) by default. Benchmark modes explicitly select their algorithm, preserving a naive reference:

```sh
./build-release/coresim --benchmark collision-naive --bodies 10000 --steps 10 --warmup 2 > naive.csv
./build-release/coresim --benchmark collision-spatial --bodies 10000 --steps 10 --warmup 2 --cell-size 3 > spatial.csv
```

Both use the same separated-sphere layout, timestep, integration, detector, solver, build, and instrumentation. No speedup should be inferred by comparing different machines or build configurations. Re-run both modes at the same revision; the original Milestone 7 files above remain historical evidence.

Current output uses **schema 2**, preserving the first 18 columns and appending `cell_size`, `hash_build_ms`, `query_ms`, `total_collision_ms`, and `pair_reduction_percent`. Cell size is a positive finite float (default 3); it is reported but unused in naive mode. Naive hash-build time is zero, and naive query time is its pair-enumeration duration. Hash query time includes duplicate removal, sorting, and pair-batch filling; broad phase equals hash build plus query. Total collision time includes narrow phase and response, excluding integration. Reduction uses all N(N−1)/2 collider pairs as its denominator, with zero reduction for fewer than two colliders.

`--scene paired-spheres` places groups of two spheres 1.5 units apart on a lattice of spacing 6, using ceil(N/2) sites and leaving the final sphere unpaired for odd N. All bodies remain dynamic with zero gravity, velocity and restitution. Positional correction separates each pair toward the solver's penetration tolerance. Unlike the default separated scene, positions evolve during warmup; both modes execute the same ticks from the same initial state. This scene exercises N/2 independent contacts, not dense piles or arbitrary stacks. For even N, the expected initial contact count is N/2 per tick and correction rechecks are 2N. Cell size affects false positives and cost. Globally large shapes, planes, or dense occupancy can reduce or eliminate the grid's advantage; worst-case work remains quadratic.

Measurements below use three independent invocations per mode/workload, with two warmup and ten measured ticks each. Algorithm order alternates naive/hash, hash/naive, naive/hash to reduce a fixed ordering bias. Runs are sequential after test/build jobs finish. All 30 ticks per algorithm/workload are retained; summaries use their median and min/max. Speedup is median naive physics time divided by median hash physics time, not an average of per-frame ratios. Candidate reduction is measured independently of runtime improvement.

### Measured comparison — October 9, 2026

Revision `8bdfceee24193f03bb9e51f59c7f5bc2e214a5db`, Apple M1 (8 logical CPU cores), 8 GiB unified RAM, macOS 26.3.1(a) build 25D771280a, Apple Clang 21.0.0, CMake 3.31.6, arm64 Release `-O3 -DNDEBUG`, no sanitizers. The integrated GPU is unused. Default cell size 3. CPU affinity, thermal state, and ordinary desktop background load were uncontrolled. These are observed local results for the specified workloads, not general engine throughput guarantees.

| Scene | Bodies | Naive median physics (ms) | Hash median physics (ms) | Physics speedup | Hash candidates/tick | Candidate reduction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| separated-spheres | 1,000 | 5.836 | 0.519 | 11.25× | 10,476 | 97.902703% |
| separated-spheres | 5,000 | 149.133 | 2.692 | 55.39× | 57,354 | 99.541076% |
| separated-spheres | 10,000 | 587.896 | 5.264 | 111.69× | 117,845 | 99.764286% |
| paired-spheres | 10,000 | 591.414 | 5.306 | 111.47× | 5,000 | 99.989999% |

| Scene / bodies | Naive physics min–max (ms) | Hash physics min–max (ms) | Median hash build (ms) | Median hash query (ms) | Median hash total collision (ms) |
| --- | ---: | ---: | ---: | ---: | ---: |
| separated-spheres / 1,000 | 5.750–6.264 | 0.501–0.563 | 0.263 | 0.147 | 0.511 |
| separated-spheres / 5,000 | 145.844–165.163 | 2.603–3.068 | 1.252 | 0.852 | 2.651 |
| separated-spheres / 10,000 | 582.158–638.557 | 5.207–5.474 | 2.390 | 1.698 | 5.183 |
| paired-spheres / 10,000 | 585.002–672.048 | 5.165–5.669 | 3.549 | 0.749 | 5.225 |

Every one of the 240 measured rows passed workload/counter validation. The separated scene had zero contacts in both modes. The paired scene retained 5,000 contacts and 20,000 correction rechecks per tick in both modes. The naive reference performed 49,995,000 initial checks at 10,000 bodies. Grid candidates also equal initial checks in these scenes, since every body is dynamic with a transform. Median component times do not necessarily sum to the median total.

The roughly 112× improvement at 10,000 bodies includes rebuilding the hash each tick. It demonstrates the benefit of rejecting distant pairs; it does not establish a speedup for densely overlapping bodies, large global shapes, plane-heavy scenes, or rendering. The measured hash physics tick fits an 8.33 ms budget in these two synthetic workloads, but that budget excludes rendering and other application work.

Reproduce the full comparison with the standard-library-only runner, choosing a new output directory:

```sh
python3 benchmarks/compare.py build-release/coresim benchmarks/results/my-comparison
```

The runner rejects an existing output directory, saves every raw CSV, validates expected counters, records the exact command order, and computes summary statistics without trimming samples. [Raw trials](benchmarks/results/m8-m1/), [machine metadata](benchmarks/results/m8-m1/metadata.json), [commands](benchmarks/results/m8-m1/commands.json), and [full statistics](benchmarks/results/m8-m1/summary.json) are checked in. Historical Milestone 7 results are retained separately above.
