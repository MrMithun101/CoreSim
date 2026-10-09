# CoreSim

**High-Performance 3D Physics Simulation Engine**

A C++20 systems and performance engineering project developed one measured milestone at a time. Current scope: **Milestone 9 — Built-in Profiler**. It provides custom fixed-step translational physics, generation-checked entity IDs, packed component storage, primitive collision detection and impulse response, a movable perspective camera and a 100-body sphere/box demo, move-aware RAII GPU resources, file-based shaders, depth testing, frame timing, and tests. A deterministic spatial hash accelerates broad-phase collision detection; a live ImGui CPU profiler exposes timing bottlenecks. Angular dynamics and GPU compute are future milestones. A headless CSV benchmark compares spatial hashing with the retained all-pairs baseline; see [measurements and methodology](BENCHMARKS.md).

![Sphere and box collision scene rendered by CoreSim](docs/images/collisions.png)

## Build and run

Requires CMake 3.25+, Git, GCC or Clang with C++20 support, and OpenGL 3.3. Linux is the primary target; macOS is also supported by the foundation. A graphical session is required for the interactive demo; benchmarks run headlessly.

Ubuntu/Debian dependencies:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake git xorg-dev libgl1-mesa-dev
```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/coresim
```

The scene contains 50 spheres and 50 axis-aligned boxes falling onto static floor supports. They bounce and settle into two-body stacks. Press **R** to reset the dynamic bodies. Use **WASD** to move, **Q/E** to descend/ascend, and **hold right mouse** to look. Release right mouse to restore the cursor. Escape or the close button exits. Its title updates twice per second with simulation time, dropped physics time, average FPS, delta time, and the last completed frame duration. A shutdown summary appears on standard output. Vertical synchronization is enabled; frame duration includes presentation waiting and is not a rendering benchmark.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
./build-release/coresim
```

On this verified local setup, CMake is temporarily available via `export PATH="/tmp/coresim-build-tools/bin:$PATH"`. For long-term macOS use, install Xcode command-line tools and CMake normally, then use the same commands. Apple's deprecated OpenGL implementation is sufficient for this foundation. HIP is not part of this milestone.

Presets `debug`, `release`, `asan`, and `tsan` use separate directories:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
./build/debug/coresim
```

## Live CPU profiler

The interactive app shows an ImGui table for Frame, Physics, Broad Phase, Narrow Phase, Solver, Rendering, and Presentation. Each row tracks calls, total, average, minimum, and maximum milliseconds. Use **Collect samples** to pause/resume and **Reset statistics** to clear measurements. See [profiler timing boundaries and design](docs/profiler.md); these are CPU wall timings, not GPU timings.

## Collision benchmarks

```sh
./build-release/coresim --benchmark collision-naive --bodies 10000 --steps 10 --warmup 2 > naive-10000.csv
./build-release/coresim --benchmark collision-spatial --bodies 10000 --steps 10 --warmup 2 > spatial-10000.csv
```

The standalone `coresim_benchmark` accepts the same arguments and is available with `CORESIM_BUILD_APP=OFF`. See [BENCHMARKS.md](BENCHMARKS.md) for counter definitions, timing boundaries, workload limits, and measured results.

## Dependencies

CMake FetchContent downloads GLFW **3.4**, GLM **1.0.1**, Catch2 **3.7.1**, and Dear ImGui **1.91.9b**, pinned to verified immutable commits. GLFW provides window/context creation; Catch2 provides tests. GLM supplies matrix math; the generated GLAD loader is vendored with its license. Dear ImGui supplies the profiler panel and official GLFW/OpenGL backends. OpenGL comes from the platform. Upstream dependencies retain their own licenses. No physics engine or prebuilt engine systems are used.

Initial configuration requires network access. Alternatively, use installed CMake packages with `-DCORESIM_FETCH_DEPENDENCIES=OFF` (GLFW >=3.4, GLM >=1.0, and Catch2 3), plus `CORESIM_IMGUI_SOURCE_DIR` pointing to Dear ImGui 1.91.9b sources for application builds, or predownloaded sources with `FETCHCONTENT_SOURCE_DIR_GLFW`, `FETCHCONTENT_SOURCE_DIR_GLM`, `FETCHCONTENT_SOURCE_DIR_CATCH2`, and `FETCHCONTENT_SOURCE_DIR_IMGUI`. Linux defaults to GLFW's X11 backend. Warnings apply only to CoreSim targets. `-DBUILD_TESTING=OFF` omits Catch2 and tests.

## Tests

Default CTest checks timing, camera/transform/scene math, entity lifecycle and component storage, fixed-step physics, primitive contacts, collision response, resting stability, and CLI handling without opening a window. The CPU tests use explicit time inputs and never sleep. GLM is required even for headless scene tests. A build without any GLFW/OpenGL dependency is available:

```sh
cmake -S . -B build-headless -DCORESIM_BUILD_APP=OFF
cmake --build build-headless --parallel
ctest --test-dir build-headless --output-on-failure
```

To run the OpenGL ownership, shader-failure, pixel/depth, and bounded window lifecycle tests:

```sh
cmake -S . -B build -DCORESIM_WINDOW_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/coresim --frames 120
```

Linux CI uses `LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure`, with `xvfb` installed. GitHub Actions covers GCC/Clang Debug/Release and headless sanitizers; its remote results must be checked after a push.

```sh
cmake --preset asan -DCORESIM_BUILD_APP=OFF
cmake --build --preset asan --parallel
ctest --preset asan
```

`CORESIM_SANITIZER=address` enables ASan plus UBSan; `thread` enables TSan separately on supported hosts; `none` is the default. These instrument CoreSim targets, not third-party code. TSan is provided for later concurrency work; this milestone is single-threaded.

## Architecture

- `Application` owns `GlfwRuntime`, `Window`, `Renderer`, camera, and scene. Reverse destruction releases GPU resources before the context and runtime.
- `Window` handles native events, context setup, framebuffer size, and presentation. OpenGL rendering lives in `coresim_renderer`.
- `VertexBuffer`, `IndexBuffer`, `VertexArray`, and `Shader` are noncopyable, movable RAII owners. Shader compilation/link failures include file paths and driver diagnostics.
- `World` owns stable entity IDs and packed transform, mesh-reference, demo-spin, rigid-body, and collider components. Destruction invalidates stale IDs and removes components.
- `Renderer` borrows a const World and draws entities with both mesh and transform components. It owns shared GPU geometry; camera and CPU scene state remain outside the renderer.
- `PhysicsSystem` integrates gravity, acceleration, and forces using semi-implicit Euler, then resolves supported contacts using normal impulses and penetration correction. `FixedStepper` runs at 120 Hz with bounded catch-up; renderer and physics borrow World without owning each other’s state.
- `FrameTimer` uses a steady clock. Delta is start-to-start; frame duration covers event processing through buffer swap. FPS averages completed frames over elapsed runtime.

See [spatial-hash design](docs/spatial-hash.md), [collision geometry and solver limits](docs/collisions.md), [physics and timestep policy](docs/physics.md), [entity storage and lifecycle](docs/entities.md), [camera/scene design and controls](docs/scene.md) and [renderer design](docs/renderer.md) for ownership contracts, the draw pipeline, shader paths, and testing rationale. Format C++ files with the checked-in `.clang-format` configuration.

Shaders are copied into the build directory and found independently of the working directory. Rebuild and restart after changing them. For a relocated executable or custom shaders:

```sh
./build/coresim --shader-dir /absolute/path/to/shaders
```

See [verification notes](docs/verification.md) for actual local results and commands. The screenshot above is an actual framebuffer capture, not an illustration or performance benchmark.

**Next: Milestone 10 — Data-Oriented Performance Work**, only on explicit request.
