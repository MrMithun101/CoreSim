# HIP compute foundation (Milestone 16)

CoreSim now includes an optional AMD HIP vector-add workload and a portable CPU reference. This is a learning and integration foundation: physics and Particle Storm still execute on the CPU. No GPU speedup, transfer-bandwidth, kernel-time or device-execution result is claimed. No compatible GPU is available to the project owner; hardware validation remains pending.

## Run without ROCm

The default build needs no HIP headers, runtime or compiler. Both commands below run without a graphics context:

```sh
./build/coresim_gpu_demo --elements 1025
./build/coresim --gpu-demo vector-add --backend cpu --elements 1025
./build/coresim_gpu_demo --info
```

The default backend is `cpu`. Output labels the actual backend, count, full-reference verification and checksum. `--backend hip` never falls back to CPU: it fails if the backend was not compiled, the runtime cannot enumerate devices, the requested device is unavailable, or any allocation/copy/launch/execution fails. `--info` only inspects availability and returns successfully even when HIP is unavailable; it does not execute a workload. `--help` also does not execute. Unknown, duplicate, malformed and mixed application/benchmark arguments are rejected.

The standalone command also accepts `--gpu-demo vector-add`; the application requires that dispatch flag. Counts range from zero through 16,777,216. The cap bounds this demonstration's host allocations (four float arrays, about 256 MiB at the limit) and HIP allocations (three arrays, about 192 MiB). Allocation may still fail under memory pressure. The library API has no demonstration cap, but validates byte-count overflow and device grid limits.

## Optional Linux AMD ROCm build

Use CMake 3.25+ and a ROCm C++20 toolchain on Linux. The compile CI uses ROCm 6.3.3 and CMake 3.31.6. macOS and NVIDIA HIP are outside this milestone's supported build configuration. Use a separate build directory; host sanitizer configurations deliberately require HIP to be off.

```sh
cmake -S . -B build-hip -DCMAKE_BUILD_TYPE=Release \
  -DCORESIM_BUILD_APP=OFF -DCORESIM_ENABLE_HIP=ON \
  -DCMAKE_HIP_COMPILER=/opt/rocm/llvm/bin/clang++ \
  -DCMAKE_PREFIX_PATH=/opt/rocm -DCMAKE_HIP_ARCHITECTURES=gfx1100
cmake --build build-hip --parallel 2
./build-hip/coresim_gpu_demo --info
./build-hip/coresim_gpu_demo --backend hip --device 0 --elements 1048579
```

**Replace `gfx1100` with your GPU's supported architecture.** CI uses it solely to compile a concrete device code object without GPU discovery. The runtime inspector reports device names and architecture strings; an image compiled for a different GPU can fail at launch. Build with the ROCm Clang compiler, not the `hipcc` wrapper: CMake's native HIP language manages device compilation. See [CMake's HIP compiler selection](https://cmake.org/cmake/help/latest/envvar/HIPCXX.html).

The `coresim_gpu` library exposes `cpu_vector_add`, `hip_vector_add` and `hip_status`. HIP support is isolated in one translation unit. The CPU implementation and unavailable-backend implementation compile on the normal project toolchain. The headless executable links no renderer and opens no window. The interactive application's early command dispatch calls the same parser and runner before GLFW initialization.

## Memory and execution model

1. **Host memory:** input and output `std::vector<float>` objects reside in ordinary pageable host memory. They are not GPU pointers. Inputs remain owned by the caller and must stay unchanged throughout the synchronous call.
2. **Device memory:** three private move-aware RAII buffers allocate device storage with `hipMalloc`. Two synchronous `hipMemcpy` operations copy inputs host-to-device. No managed memory, pinned host buffers, OpenGL interop or shared-address assumption is used.
3. **Kernel, grid, block, thread:** each GPU thread computes one output element. A one-dimensional grid covers the input with blocks of up to 256 threads (bounded by the device limit). Thread index is `blockIdx.x * blockDim.x + threadIdx.x`, calculated in `size_t`. Rounded-up grids include a bounds check so partial final blocks cannot read or write beyond the arrays. Empty input launches nothing, but a HIP request still requires a working runtime and valid device.
4. **Synchronization:** the launch uses the default stream and is asynchronous with respect to the host. `hipGetLastError` checks submission; `hipDeviceSynchronize` checks completed execution before results are copied back. This device-wide barrier can wait for unrelated work, so the API makes no overlap or concurrency-performance promise. See AMD's [asynchronous execution guide](https://rocm.docs.amd.com/projects/HIP/en/docs-6.3.3/how-to/hip_runtime_api/asynchronous.html).
5. **Return and ownership:** a synchronous device-to-host copy populates the output vector. Normal completion checks each free and restores the calling thread's previously selected device. Exception unwinding frees any successfully allocated buffers before restoring the device; destructor cleanup failures are logged without throwing. The caller must not reset the device concurrently. Pageable host allocations are deliberate; see AMD's [host-memory documentation](https://rocm.docs.amd.com/projects/HIP/en/latest/how-to/hip_runtime_api/memory_management/host_memory.html).

Input validation rejects mismatched sizes, nonfinite operands and sums outside the finite float range before accessing HIP. This incurs a host pass and is part of the current contract. The demonstration uses bounded multiples of one quarter and one half, so CPU/GPU results should compare exactly. General float inputs are not promised bitwise equivalence across different GPU floating-point modes, particularly subnormals. The checksum is a diagnostic, not the correctness oracle: every returned element is compared with the CPU reference.

## Validation boundaries

Default tests cover CPU addition, immutability, empty input, launch-boundary sizes, nonfinite and overflowing inputs, CLI validation, CoreSim dispatch, and the unavailable backend. ROCm CI compiles and links the real kernel, demonstration and hardware-test executable, then runs CPU-only integration tests. It has no GPU and does **not** run or skip a hardware test while reporting it passed.

On a compatible GPU machine, explicitly enable device execution:

```sh
cmake -S . -B build-hip -DCORESIM_HIP_DEVICE_TESTS=ON
cmake --build build-hip --parallel 2
ctest --test-dir build-hip -L gpu --output-on-failure
```

The device suite requires successful enumeration and fails when no device is available. It compares all results for 0, 1, 255, 256, 257, 1,025 and 1,048,579 elements on every enumerated device, repeats allocations/execution three times, checks input preservation, checks caller-device restoration, and rejects invalid device indices. Allocation-failure injection and device-fault recovery are not covered. Actual kernel execution, device cleanup behavior and cross-device restoration remain unverified until this suite runs on hardware.

Milestone 17 particle simulation and CPU/single-thread/multithread/HIP performance comparisons are intentionally deferred. Those measurements will need separate kernel and transfer timings, warmups, workload sizes, machine metadata, and an end-to-end comparison.
