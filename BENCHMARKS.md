# Collision baseline

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

Candidate generation enumerates each i<j pair in component order into a fixed 4096-pair scratch batch. Narrow phase consumes each batch before enumeration resumes. This preserves O(N²) work and pair order with bounded candidate storage (64 KiB on the measured 64-bit host), instead of storing tens of millions of pairs. Contact storage remains proportional to detected contacts. The interactive solver uses the same enumeration; timing is opt-in via `CollisionStats*`, with no clock reads when omitted.

CSV schema version 1 emits one row per measured tick, with no discarded outliers:

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

Results and machine details are added after running the checked-in Release executable. These are a baseline, not a speedup claim. Spatial hashing belongs to Milestone 8.
