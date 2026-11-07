# CUDA particle integration — Milestone 20

The particle runner can now execute the [constant-acceleration workload](particle-workload.md) on an NVIDIA GPU. The default build is still CPU-only. **The CUDA implementation has not run on a physical GPU in this project.** There are no CUDA timings or speedup results. The [measured CPU baseline](particle-cpu-baseline.md) remains the comparison point for Milestone 21.

## Build and inspect

The ordinary Mac/Linux build requires no CUDA SDK, driver, or headers. `coresim_particle_runner --cuda-info` reports whether CUDA was compiled and which devices the runtime can enumerate. Inspection can succeed with a diagnostic and zero devices. A computation requesting CUDA must succeed on the selected device or exit nonzero; it never falls back to the CPU.

For the optional backend, use a separate Linux build with NVCC 12+ and a compatible C++20 host compiler. The CI configuration uses NVIDIA's [CUDA 12.6 packages](https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/) for Ubuntu 24.04; see the [NVIDIA installation guide](https://docs.nvidia.com/cuda/archive/12.6.3/cuda-installation-guide-linux/index.html) for toolchain and driver requirements. A driver and compatible GPU are needed for execution, not offline compilation.

```sh
cmake -S . -B build-cuda -DCMAKE_BUILD_TYPE=Release -DCORESIM_BUILD_APP=OFF \
  -DCORESIM_ENABLE_CUDA=ON -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.6/bin/nvcc \
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-13 -DCMAKE_CUDA_ARCHITECTURES=80
cmake --build build-cuda --parallel
./build-cuda/coresim_particle_runner --cuda-info
./build-cuda/coresim_particle_runner --backend cuda --device 0 --particles 100000 --steps 120 --seed 7
```

Replace the compiler paths and **80** with the toolchain and architecture appropriate to your machine. CI targets compute capability 8.0 without attempting device discovery. An incompatible device image can fail at launch even if enumeration succeeds. On a multi-GPU machine the hardware suite exercises every enumerated GPU; build for all of their architectures or restrict the visible devices. CUDA builds reject the project's host sanitizer options. The existing AMD HIP vector-add workload remains a separate option.

## What a batch does

The host generates exactly the same seeded 24-byte AoS particle records as the serial reference. The library entry point `integrate_cuda(initial, acceleration, steps, device)` accepts 0–100,000 finite particles, finite acceleration, 0–10,000 ticks, and a nonnegative device index. Empty or zero-step batches still require a valid CUDA runtime/device. The CLI requires at least one particle and one tick, rejects `--workers` for CUDA, and defaults to device 0.

1. Validate the inputs, select the requested device, and create a private nonblocking stream. Upload the initial state once and clear a device-side failure flag.
2. Launch one one-dimensional grid per fixed tick. A thread updates at most one particle, using the new velocity to advance position at float32 `1/120` s. Tail threads exit before indexing. Stream ordering preserves tick order. An in-place update is valid for this independent-particle workload; it would not suffice for arbitrary particle collisions.
3. Keep the particle array resident throughout the batch. If any component becomes nonfinite, set the sticky failure flag. Check submission errors after each launch and execution errors at stream synchronization. On failure no partial state is returned.
4. Read the flag and download the final particle array. Private allocations/stream are released on success and exception paths. Restore the calling thread's previous device; cleanup errors during unwinding are logged. Host inputs are never modified. Callers must keep input storage stable during the synchronous call and must not reset a device concurrently.

This is a synchronous host API. Milestone 21 adds optional event timing for the [same-host study](cuda-profiling.md). Memory is device-resident **within** a batch, not retained between calls. Transfers use pageable host buffers. Milestone 20 does not implement transfer overlap, graphics interop, collisions, or the Studio backend selector.

## Correctness and remaining validation

The CUDA runner performs an untimed serial replay and compares **every final position and velocity scalar**. A successful run reports the backend, device, count, seed, ticks, maximum absolute error, tolerances, and diagnostic digest. Any mismatch fails the run. For reference value `r`, both values must be finite and `abs(actual-r) <= 0.0002 + 0.00002*abs(r)`. These are provisional bounds for the baseline workload, not observed GPU error measurements or a universal guarantee. NVCC contraction is disabled for the kernel; the CPU compiler may make different floating-point choices. Existing CPU parity tests remain exact.

Hosted CI builds the actual device code, links the runner and device tests, and runs only hardware-independent tests. On compatible hardware explicitly enable execution:

```sh
cmake -S . -B build-cuda -DCORESIM_CUDA_DEVICE_TESTS=ON
cmake --build build-cuda --parallel
ctest --test-dir build-cuda -L cuda --output-on-failure
```

These tests **fail**, rather than skip, when no GPU is available. They cover empty and zero-step inputs, partial blocks, 1K/10K/100K workloads, repeated batches, non-default acceleration, overflow rejection, host input preservation, invalid device selection and caller-device restoration. A separate CLI test exercises final-state verification. Until these run on hardware, device correctness, resource behavior and performance remain unverified. Milestone 21 needs actual GPU results and separate kernel, transfer, and end-to-end measurements.
