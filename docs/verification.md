# Verification

## Milestone 14 — October 13, 2026

Baseline checkpoint `c965703` adds an isolated rendering benchmark and serial comparison runner. Instancing checkpoint `2dfd015` adds streamed per-mesh matrix attributes, retained CPU/GPU capacities, one indexed instanced call per nonempty mesh type, actual submission counters and a selectable individual-draw reference. The interactive renderer defaults to instancing. GPU resources remain context-bound RAII owners; scene/physics ownership is unchanged.

All 38 local CTest entries passed in Release, Debug and ASan/UBSan, including both benchmark modes with mixed geometry, invalid CLI inputs, renderer/UI tests and the window smoke test. The final added cross-batch depth regression was then rechecked in all three renderer builds: 148 assertions in 12 cases each. Builds emitted no CoreSim compiler/linker warnings or sanitizer diagnostics. GLM's existing upstream CMake deprecation warning remains. Renderer tests compare complete images and cover growth to 1,200 mixed objects, buffer moves, mutation, shrink, missing/destroyed components, mesh changes, zero-size targets, and overlapping meshes at distinct depths.

On the Apple M1 host, the baseline retained 270 measured rows and the paired cube/mixed studies retained 1,080 rows across 36 comparison invocations. All counts, timings and exact final image checksums passed the runner's validation. Counts were 1,000, 10,000 and 25,000; each mode/count/geometry used three trials with ten warmup and thirty measured frames. See [all measurements, machine metadata and interpretation](rendering-scalability.md). These are completed offscreen rendering measurements with no physics, presentation or UI; derived FPS is not full-application FPS. No GPU timer-query or hardware-counter claim is made.

No Milestone 15 demo-scene work is included.

## Milestone 13 — October 12–13, 2026

Core checkpoint `0577bb7` adds shape-derived principal/world inertia tensors, torque and offset forces, normalized quaternion integration with world angular momentum, oriented box manifolds and rotated broad-phase bounds, angular normal impulses, and Coulomb contact friction. The [checkpoint CI run](https://github.com/MrMithun101/CoreSim/actions/runs/38341401469) passed all six compiler/display/sanitizer jobs. The final integration tilts/spins demo boxes, restores angular state on reset, prevents animation from overwriting rigid-body orientation, and extends concurrency/state checks to angular motion.

The integration commit `a9854f6` also passed [all six CI jobs](https://github.com/MrMithun101/CoreSim/actions/runs/38342192117).

All 28 local CTest entries passed in Debug, Release and ASan/UBSan. Final builds emitted no CoreSim compiler/linker warnings. The CPU suite passed 378,798 assertions in 58 cases; parallel physics passed 1,196,243 assertions in seven cases; the existing jobs suite passed 36,129 assertions in ten cases. The renderer passed 71 assertions across nine cases with image capture (70 without capture), including actual framebuffer changes from applied torque. The captured initial scene was visually inspected and saved as `docs/images/angular.png`; it is not a performance measurement.

A separate headless ThreadSanitizer build passed five consecutive runs of each concurrency suite without race reports. Angular tests cover mass/shape inertia changes, rotated tensors, torque/force-at-offset response, normalized orientation, 20 seconds of asymmetric free rotation with bounded momentum/energy error, analytical frictionless corner impact, two-body momentum transfer, material validation, sliding/spin friction, tilted/skew box geometry, rotated planes, angular reset and invalid inputs. The existing five-second 100-box floor and ten-second mixed-demo stability checks pass. Seeded broad-phase tests now use rotated geometry, and serial/parallel plus naive/spatial checks compare angular state as well as linear state.

A final regression covers the largest finite SAT overlap: the first valid axis must select a contact feature even when its depth equals the initial maximum. The updated Release CPU suite passed 378,802 assertions in 59 cases. Temporary dependency cleanup prevented rebuilding Debug/ASan locally for this final fix; the [final code CI run](https://github.com/MrMithun101/CoreSim/actions/runs/38415713690) for `ea23d55` passed all six jobs, including GCC/Clang Debug/Release display tests, ASan/UBSan, and ten ThreadSanitizer repetitions of each concurrency suite.

Current collision/layout/parallel CSV schemas (3/2/2) were exercised; historical packed experiments reject angular motion/torque. Earlier performance data remains historical. See [angular equations, ownership, numerical tolerances and limits](angular-dynamics.md). No Milestone 14 rendering-scalability work is included.

## Milestone 12 — October 12, 2026

Core checkpoint `d3ce3dd` adds synchronous parallel integration through the fixed worker pool, disjoint bounded body ranges, a configurable grain/inline fallback, admission-failure draining, and worker-side nesting rejection. The [checkpoint CI run](https://github.com/MrMithun101/CoreSim/actions/runs/38339137908) passed all six jobs: GCC/Clang Debug/Release with display tests, headless ASan/UBSan, and ten repetitions of both concurrency suites under ThreadSanitizer. The application now supplies its pool; its small demo remains inline. The [integration CI run](https://github.com/MrMithun101/CoreSim/actions/runs/38339935480) for `c807680` also passed all six jobs, including ten ThreadSanitizer repetitions of each concurrency suite.

All 28 CTest entries passed locally in Debug, Release and ASan/UBSan, including the display suites and valid/invalid benchmark CLI cases. Final builds emitted no CoreSim compiler/linker warnings. The parallel suite passed 750,385 assertions in seven cases; the existing job suite passed 36,129 assertions in ten cases and the CPU suite passed 244,947 assertions in 46 cases. A separate local ThreadSanitizer build passed five repetitions of both concurrency suites with no race reports. Tests cover exact per-body equivalence across 1/2/4/8 workers and both traversal modes, supported contacts under both broad phases, structural churn between ticks, range bounds, missing/static bodies, exceptions, closed/concurrent shutdown, inline fallback and nested-worker rejection.

The headless Release scaling studies retain 21,250 measured rows across 425 invocations, plus 60 pre-change baseline rows. Every formal scaling row passed workload/timing checks and exact serialized state-checksum/collision-counter comparison against other configurations. All trial medians, raw min/max values, commands, compiler/revision/binary metadata and machine details are retained in the [parallel-physics report](parallel-physics.md). Measured benefits apply to independent integration/free-flight workloads; collision-dominated ticks show little total improvement. The report includes small-scene overhead, grain-size tradeoffs, heterogeneous cores, observed variability and limits of cache/bandwidth/false-sharing inferences. No hardware-counter evidence or general frame-rate improvement is claimed.

Physics still joins before serial collision solving, profiler updates and rendering. No Milestone 13 angular dynamics work is included.

## Milestone 11 — October 12, 2026

Core checkpoint `f43f12d` adds a fixed worker pool with blocking FIFO admission, sealed completion groups, shared state lifetime, exception propagation after sibling completion, and draining shutdown. The application owns a pool that joins before scene/graphics teardown; physics, rendering, and profiling remain single-threaded.

The core checkpoint passed Linux CI. The [integration CI run](https://github.com/MrMithun101/CoreSim/actions/runs/38322238047) for `a83ca23` passed all six jobs: GCC/Clang Debug/Release with display tests, headless ASan/UBSan, and ten consecutive ThreadSanitizer stress runs on macOS.

All 18 CTest entries passed locally in Debug, Release, and ASan/UBSan. The final job suite passed 36,129 assertions in ten cases; its final added group-isolation/non-standard-exception case was rechecked in all three builds. The existing CPU suite remains 244,947 assertions in 46 cases. Final builds emitted no CoreSim compiler/linker warnings or sanitizer diagnostics.

A separate headless Apple Clang ThreadSanitizer build instrumented both the jobs library and test executable. The final concurrency suite passed ten consecutive CTest runs with no race reports (log: `/tmp/coresim-m11-tsan-final.log`, temporary local evidence). CI adds the same repeated job suite on `macos-latest`, alongside the existing Linux compiler and ASan/UBSan matrix. Sanitizers exercise schedules rather than proving correctness.

Stress cases include 12,000 unique jobs at each of one/two/four workers, six concurrent producers, multiple waiters, concurrent submit/shutdown and shutdown/shutdown races, group sealing races, exception propagation, queued-work destruction, repeated wakeups/pool lifecycles, blocked unrelated groups, worker blocking-call rejection, descendant submission, and completion after callable capture release. A 60-second test timeout bounds deadlock failures. Idle workers use a condition-variable predicate, with no polling or spin loop. Constructor resource-exhaustion cleanup is implemented but resource exhaustion was not deliberately induced.

See [job-system contracts and usage](jobs.md). No parallel-physics throughput claim or Milestone 12 work is included.

## Milestone 10 — October 9, 2026

Experiment checkpoints `56eda63` and `ccac0f4` passed Linux CI. The [final integration CI run](https://github.com/MrMithun101/CoreSim/actions/runs/37967997169) for `571db77` passed all five jobs: GCC/Clang Debug/Release with display tests and headless ASan/UBSan. The final decision retains lookup traversal and the existing component layout in production: dense traversal improved isolated integration but did not show a consistent complete-physics benefit in two measured runs. AoS/SoA work buffers remain experimental because total gather/integrate/scatter costs exceeded the reference.

Debug, Release, and ASan/UBSan each passed all 17 local CTest entries, including six layout/full-physics CLI modes, malformed-input handling, the existing collision benchmarks, profiler controls, renderer, and window smoke tests. The CPU suite passed 244,947 assertions in 46 cases. Final builds emitted no CoreSim compiler/linker warnings or sanitizer diagnostics. New coverage checks per-body integration equivalence across ordered/shuffled joins and structural churn, static and missing-transform bodies, invalid timesteps/nonfinite outputs/dynamic planes, exact full-physics agreement during contacts, and read-only entity identity with mutable values in the dense visitor.

The integration comparison retained 6,400 measured rows. Initial and repeated complete-physics comparisons retained another 3,200 each; all 12,800 passed workload/state-checksum validation. Every raw row, command, compiler/machine/revision metadata, median/min/max summary, and the adoption decision is preserved in [the data-layout investigation](data-layout.md). These are synthetic mixed-body steady-state workloads, not general engine throughput guarantees. No samples or unsuccessful comparisons were discarded.

This host is macOS and has no Linux perf tool. Cycles, instructions, cache misses and branch misses were not measured; the report documents reproducible Linux perf commands and their process-wide measurement limits without claiming hardware-counter improvements. No Milestone 11 threading work is included.

## Milestone 9 — October 9, 2026

Profiler core checkpoint `97bce30` passed Linux CI. The [integration CI run](https://github.com/MrMithun101/CoreSim/actions/runs/37948569857) for `5afa810` passed all five jobs: GCC/Clang Debug/Release with the new ImGui display tests and headless ASan/UBSan. UI integration `5afa810` adds pinned Dear ImGui 1.91.9b with official GLFW/OpenGL3 backends, live inclusive CPU statistics, collection pause/resume, and reset. The headless configuration does not fetch or link ImGui.

Debug, Release, and ASan/UBSan each passed all ten local CTest entries. The CPU suite passed 189,122 assertions across 42 cases. Final builds emitted no CoreSim compiler/linker warnings or sanitizer diagnostics. Deterministic tests cover aggregation, invalid samples, nested scopes, exception unwinding, reset/enable generation boundaries, clock-read avoidance when disabled, and unchanged profiled/unprofiled physics results.

The display test passed 28 assertions (29 with optional image output), checking framebuffer content, GL errors, repeated context creation/destruction, and actual pause/resume/reset actions through ImGui input events. Its framebuffer capture was visually inspected and saved as `docs/images/profiler.png`; it uses synthetic timing samples, not benchmark results. Existing renderer and application smoke tests also passed. The interactive 120-frame diagnostic run recorded 120 Frame/Rendering/Presentation samples and 132 Physics/Broad Phase/Narrow Phase/Solver samples, matching its fixed-tick count, with zero dropped physics time. This was execution/instrumentation verification, not a controlled performance measurement.

See [profiler design and timing boundaries](profiler.md). CPU wall time is inclusive; rendering reports CPU submission rather than GPU execution, and the separate presentation row includes VSync/driver waiting. Statistics are cumulative since reset and refreshed at 4 Hz. No worker-thread profiling or Milestone 10 layout optimization is included.

## Milestone 8 — October 9, 2026

Spatial-hash implementation checkpoint `aa1de03` passed Linux CI. The [integration CI run](https://github.com/MrMithun101/CoreSim/actions/runs/37946921675) for `8bdfcee` also passed all five jobs: GCC/Clang Debug/Release with display tests and headless ASan/UBSan. Raw results are preserved in `7fbf018`. Integration checkpoint `8bdfcee` adds schema-2 metrics, explicit naive/spatial benchmark modes, configurable cell size, a paired-contact workload, and a reproducible comparison runner.

Debug, Release, and ASan/UBSan each passed all nine local CTest entries, including renderer and window smoke tests. The CPU suite passed 189,098 assertions in 39 cases. Builds emitted no CoreSim compiler/linker warnings or sanitizer diagnostics. New tests cover exhaustive-contact containment for seeded mixed primitives, negative coordinates and touching boundaries, three cell sizes, unique sorted candidates, repeated queries, planes, large/huge-coordinate fallbacks, missing transforms, and rebuilds after component mutation. A 600-tick mixed demo comparison requires exact agreement between naive and spatial contact counts, positions, and velocities. Benchmark tests validate both modes, CSV fields, cell-size parsing, candidate reduction, and paired contacts.

The Release comparison ran 24 invocations (three trials per mode across four workloads), two warmup and ten measured ticks each. All 240 measured rows passed the runner's workload/counter checks. Actual results, untrimmed raw CSV, metadata, methodology and limits are in [BENCHMARKS.md](../BENCHMARKS.md). No performance thresholds are asserted in unit tests.

## Milestone 7 — October 8, 2026

The [integration CI run](https://github.com/MrMithun101/CoreSim/actions/runs/37869015935) for `33fc965` passed all five jobs: GCC/Clang Debug/Release with software-rendered display tests, and headless ASan/UBSan including the standalone benchmark. Implementation checkpoints are `ac328fc` (batched enumeration/metrics) and `33fc965` (CLI/CSV); `8cca3cd` preserves the actual raw measurement results.

Debug, Release, and ASan/UBSan each passed all eight local CTest entries, including renderer and window smoke checks. The CPU suite passed 54,605 assertions across 34 cases. Final builds emitted no CoreSim compiler or linker warnings. New tests cover batch-boundary pair counts, per-tick counter reset, eligibility filtering, correction rechecks, strict benchmark CLI parsing, CSV row contents and timing validity, and output failures. CTest exercises both standalone and application benchmark entry points without opening a window.

The actual Release benchmark ran 1,000, 5,000, and 10,000 bodies sequentially for two warmup and ten measured ticks each. Every measured row was checked against N(N−1)/2 candidates/checks and zero contacts. Raw CSV, machine metadata, timing definitions, limitations, and measured summaries are in [BENCHMARKS.md](../BENCHMARKS.md). Performance was measured separately from sanitizer/display test runs.

## Milestone 6 — October 8, 2026

Commits `836b3f6` (detection), `250a4a7` (response), and `837571a` (demo/rendering) passed Linux CI. The [integration run](https://github.com/MrMithun101/CoreSim/actions/runs/37867746105) passed GCC/Clang Debug/Release with software-rendered display tests and the headless ASan/UBSan job.

Debug, Release, and ASan/UBSan builds passed all five local CTest entries without CoreSim compiler warnings or sanitizer diagnostics. The CPU suite passed 54,513 assertions in 30 cases. The renderer suite passed 67 assertions in eight cases (68 when optional scene-image output was enabled). Counts include earlier milestones and are not a coverage percentage.

Primitive tests cover separated/touching/overlapping spheres and AABBs, contained boxes, coincident sphere centers, translated planes, reversed normals, unsupported pairs, constructor validation, and collider lifecycle. Response tests cover restitution, equal-mass momentum, separating contacts, inverse-mass correction, static and missing-body colliders, plane restrictions, and resting spheres. The 100-box floor workload ran five simulated seconds; the mixed 100-body two-layer demo ran ten seconds, with all dynamic bodies remaining supported and near rest within the recorded test tolerances. These are correctness workloads, not performance benchmarks or arbitrary-stack stability claims.

OpenGL tests include curved sphere silhouette coverage, the existing depth/lifecycle checks, and the mixed scene capture. The actual capture was visually inspected and saved in `docs/images/collisions.png`. A separate 360-frame demo run was used for execution/shutdown verification. UI keyboard/mouse/reset limitations remain as documented for earlier milestones.

The solver is discrete and all-pairs, with no friction, CCD, rotated boxes, sphere/box, or box/plane support. The visible AABB floor supports boxes; a colocated invisible plane supports spheres. Details are in `collisions.md`.

## Milestone 5 — October 8, 2026

Commits `b920e44` (CPU physics) and `204cf3d` (application integration) passed Linux CI. Both integration runs completed successfully; the [recorded run](https://github.com/MrMithun101/CoreSim/actions/runs/37834311413) covers GCC/Clang Debug/Release with software-rendered display tests plus headless ASan/UBSan.

Debug, Release, and ASan/UBSan builds passed all five local CTest entries without CoreSim compiler warnings or sanitizer diagnostics. The CPU suite passed 53,700 assertions in 20 cases; the renderer suite passed 63 assertions in seven cases. Counts include prior milestone tests and are not coverage percentages.

New tests check the semi-implicit discrete solution, gravity and inverse-mass force response, static and missing-transform handling, force accumulation/clearing, rigid-body lifecycle, invalid state handling, fixed-step remainders, stall time accounting, and reset behavior. Two seconds at 30/60/144 render FPS produce identical 240-step states; irregular frame durations also produce 240 steps. A GPU readback test confirms that physics-updated positions change rendering without renderer-owned simulation state.

A separate 240-frame application run exited normally and reported 260 physics ticks and 0.275308 seconds of dropped physics time. The nonzero dropped time demonstrates the configured stall policy being exercised; this run is not a performance benchmark or a claim of real-time simulation during overload. R-key edge detection was not manually verified; the reset operation itself is covered by CPU tests, including stale-ID safety. Collision handling and angular dynamics remain absent.

## Milestone 4 — October 8, 2026

Commits `40bb5a6` (storage) and `f311ebf` (integration) passed Linux CI. The [integration run](https://github.com/MrMithun101/CoreSim/actions/runs/37832701304) passed all five jobs: GCC/Clang Debug/Release with software-rendered display tests and headless ASan/UBSan.

Debug, Release, and ASan/UBSan builds passed all five local CTest entries on the macOS/Apple Clang environment below. No CoreSim compiler warnings or sanitizer diagnostics were reported. The CPU suite passed 53,042 assertions across 12 cases; the renderer suite passed 59 assertions across six cases. Most CPU assertions come from the deterministic lifecycle reference-model test; the count is not a coverage percentage.

New tests cover generation invalidation and slot reuse, repeated destruction, component attachment/replacement/removal, packed-pool compaction, const access, cleanup of all component types, and 6,000 fixed-seed lifecycle operations checked against a reference model. Demo animation remains valid after deletion, missing transforms, and adding a static entity. GPU readback verifies transform-only invisibility, rendering after mesh attachment, hiding/restoring a removed transform, removal after destruction, and an empty replacement entity.

All configurations passed the window smoke test with the entity-backed 64-cube scene. Display tests require desktop access as before. Generation exhaustion is guarded but not practically exercised to its limit. IDs remain scoped to their originating World; cross-World misuse is a documented precondition, not a detected error. No new physics behavior or performance claims were added.

## Milestone 3 — October 8, 2026

On the same macOS/Apple Clang environment described below, Debug, Release, and ASan/UBSan builds passed all five CTest entries with no CoreSim compiler warnings or sanitizer diagnostics. The CPU suite passed 307 assertions across eight cases; the renderer suite passed 50 assertions across five cases (51 with scene image capture enabled).

New checks cover camera movement and timestep independence, diagonal speed, view/projection behavior, pitch limits, transform composition, deterministic 64-object initialization, quaternion normalization over 1,000 updates, independent object rendering, and rendered output after camera motion. The multi-object framebuffer capture was visually inspected and saved as `docs/images/scene.png`.

The interactive application ran and exited cleanly after 2,427 frames. The desktop automation tool could not attach to the command-line GLFW executable, so live WASD/mouse/cursor-focus behavior was not verified through UI automation. Movement/look math and its effect on rendered output were tested independently. This distinction remains relevant for manual testing.

GitHub Actions passed the baseline commit `d84e2b8` and camera-math commit `622d2bc`, including Linux GCC/Clang Debug/Release display tests and the headless ASan/UBSan job. The scene integration commit `4e2b7b4` also passed all five Linux jobs: GCC and Clang in Debug/Release with software-rendered display tests, plus headless ASan/UBSan ([CI run](https://github.com/MrMithun101/CoreSim/actions/runs/37729629798)). Local verification used the same pinned dependency checkouts as milestone 2.

## Milestone 2 — October 8, 2026

Environment: macOS arm64, Apple Clang 21.0.0 (`clang-2100.1.1.101`), CMake 3.31.6. GLFW 3.4 and Catch2 3.7.1 use the commits recorded in `cmake/Dependencies.cmake`; GLM 1.0.1 was added at commit `0af55ccecd98d4e5a8d1fad7de25ba429d60e863`. GLAD provenance is recorded in `third_party/glad/README.md`.

| Configuration | Configure / build | CTest | CoreSim diagnostics |
| --- | --- | --- | --- |
| Debug | Passed | 5/5 passed | No compiler warnings |
| Release | Passed | 5/5 passed | No compiler warnings |
| Debug + ASan/UBSan | Passed | 5/5 passed | No compiler warnings or sanitizer diagnostics |

GLM 1.0.1 emits CMake deprecation warnings about its upstream minimum CMake policy version. These are configuration warnings, not compiler warnings in CoreSim. They are not suppressed globally.

The timing suite passed 17 assertions in four cases. The renderer suite passed 44 assertions in four cases (45 in the Debug capture run, which additionally checks image output). Tests cover ownership moves and replacement deletion, missing shader files, compilation and link failures, uniform lookup, actual color/depth output, changed pixels with rotation, depth ordering with a disabled-depth control, zero-size drawing, and viewport updates. Every configuration also passed CLI help, invalid frame-count rejection, and the five-frame window test.

The actual rendered framebuffer was captured and visually inspected; `docs/images/cube.png` shows the result. A separate 120-frame application run from `/tmp` confirmed that the default shader lookup works outside the repository and that the application shuts down normally. No performance claims are derived from frame timing in these short runs.

Display tests ran with macOS desktop access outside the tool sandbox. The milestone 1 sandboxed window test had timed out due to denied macOS desktop services; this is why display access is required. Headless timing/CLI tests need no desktop.

Not verified here: GCC/Linux execution, remote GitHub Actions, TSan, or manually pressing Escape/clicking the native close button. Sanitizers instrument host-side CoreSim targets, not dependencies or the graphics driver. The default network FetchContent path was not used for these builds: source overrides referenced the exact pinned upstream checkouts.

## Reproduce the local setup

CMake is installed temporarily in `/tmp/coresim-build-tools`; source checkouts are in `/tmp/coresim-deps`.

```sh
cd /Users/mithunselvananthan/Documents/ChatGPT/CoreSim
export PATH="/tmp/coresim-build-tools/bin:$PATH"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCORESIM_WINDOW_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_GLFW=/tmp/coresim-deps/glfw \
  -DFETCHCONTENT_SOURCE_DIR_CATCH2=/tmp/coresim-deps/catch2 \
  -DFETCHCONTENT_SOURCE_DIR_GLM=/tmp/coresim-deps/glm
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/coresim
```

For Release substitute `build-release` and `-DCMAKE_BUILD_TYPE=Release`. For sanitizers substitute `build-asan`, keep Debug, and add `-DCORESIM_SANITIZER=address`.

Optional framebuffer capture from the renderer test:

```sh
CORESIM_TEST_IMAGE=/tmp/coresim-cube.ppm ./build/coresim_renderer_tests
```

The temporary tools/checkouts may be removed by the OS. For long-term use install CMake normally, then use the README's commands in a fresh build directory to fetch dependencies into that directory.

## Milestone 1 history

Before the renderer was added, Debug, Release, and ASan/UBSan builds passed all four then-existing CTest checks, including a five-frame window test. The project was recreated at the user's request after the original directory was removed. Git was reinitialized on `main`. The user subsequently requested frequent commits and pushes; the completed baseline and camera math were committed and pushed to `main` at the supplied GitHub repository.
