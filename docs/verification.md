# Verification

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

Before the renderer was added, Debug, Release, and ASan/UBSan builds passed all four then-existing CTest checks, including a five-frame window test. The project was recreated at the user's request after the original directory was removed. Git was reinitialized on `main`. No commits, remote configuration, or pushes have been created.
