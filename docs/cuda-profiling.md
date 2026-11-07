# Run the Milestone 21 hardware study

**Status: prepared; physical NVIDIA measurements pending.** The [Mac rehearsal report](../benchmarks/results/particles-m21-cpu-rehearsal/report.md) exercises the exporter and CPU backends. It contains no GPU times or speedups. To finish Milestone 21, run the following on one supported NVIDIA Linux machine and review the resulting correctness and timing evidence.

## One hardware session

Use a Release build without sanitizers. The following matches the NVCC 12.6 CI setup; change the compiler paths and architecture to suit your toolkit and GPU. Each GPU visible to the device suite needs a compatible image; see the [CUDA setup guide](cuda-particles.md). Minimize competing work and record relevant power, clock, or scheduling settings when publishing conclusions.

```sh
cmake -S . -B build-m21-cuda -DCMAKE_BUILD_TYPE=Release -DCORESIM_BUILD_APP=OFF \
  -DBUILD_TESTING=ON -DCORESIM_ENABLE_CUDA=ON -DCORESIM_CUDA_DEVICE_TESTS=ON \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.6/bin/nvcc \
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-13 -DCMAKE_CUDA_ARCHITECTURES=80
cmake --build build-m21-cuda --parallel
./build-m21-cuda/coresim_particle_runner --cuda-info
python3 benchmarks/cuda_study.py --build build-m21-cuda --device 0 --workers 4 \
  --output /tmp/coresim-m21-hardware
```

The study checks that the three required device tests are registered and enabled, runs the CUDA-labeled CTest suite, and requires success **before measuring**. This covers all three target counts, grid boundaries, repeated execution, input preservation, overflow rejection, device restoration and the timed path. The suite exercises all visible devices; the measured trials use the selected index. A missing GPU, disabled tests, failed numerical check, invalid measurement or failed subprocess aborts the study. Results are written only after all trials validate, to a new directory. Use `--ctest /path/to/ctest` if CTest is not on PATH.

Default study: 1K, 10K and 100K particles × serial CPU, four-worker CPU and CUDA × five trials = **45 fresh processes**. All use AoS, seed 7, acceleration (0, -9.81, 0), 1/120-second simulation ticks, 60 warmup ticks and 120 measured ticks. Within each round configurations are shuffled using order seed 21. Workers, trial count, steps, warmup and seeds are configurable. Compare these CPU results with CUDA from **this same session**; the Apple M1 baseline is a separate reference.

## What each time includes

- **End-to-end (host clock):** starts with a prepared host input array and ends with a complete result vector on the host. The warmup batch is discarded; the measured batch restarts from the original input. CPU timing includes simulation construction, stepping, result materialization and simulation destruction. Its worker pool is already running. CUDA timing includes batch validation, device selection/inspection, allocations, stream and event setup, particle transfers, all launches, overflow checking, waits, cleanup and device restoration. This is warmed, instrumented batch latency, not process startup, pure CPU stepping, frame time or cold CUDA context initialization.
- **Upload and download (CUDA events):** bracket the respective particle array copies on the same private stream. Host buffers are pageable. Each transfer carries 24 bytes per particle; the small failure-flag operations are outside these intervals.
- **Kernel batch (CUDA events):** begins after the flag initialization and ends after the final particle kernel. This stream interval can include gaps between launches, scheduling interference and work submission latency. It is not a hardware-counter measurement or the sum of isolated kernel durations. Very short event intervals may round to zero. See NVIDIA's [event timing reference](https://docs.nvidia.com/cuda/archive/12.6.3/cuda-runtime-api/group__CUDART__EVENT.html).
- **Outside timing:** seeded input generation, warmup, serial correctness replay, numerical comparison and report serialization. All CPU results must match the serial replay exactly. GPU rows use the [finite per-scalar tolerance](cuda-particles.md#correctness-and-remaining-validation) and record observed maximum absolute error. An actual GPU digest need not match the CPU bit-for-bit. Reference digests must agree across equivalent inputs.

The three device intervals are **not** expected to sum to the host duration. Only end-to-end medians are used to compute speedups. This comparison deliberately uses the same input/output boundary for CPU and CUDA; the Milestone 19 CPU-only study measured a narrower stepping loop.

## Report and rehearsal

The output contains `report.md`, `metadata.json`, raw `trials.json/csv` and grouped `summary.json/csv`. The Markdown report lists median/min/max batch latency, median device phases and worst observed absolute error for each configuration. Hardware reports also show serial/CUDA and selected-worker/CUDA end-to-end ratios per count; **below 1 means CPU was faster**. Identify the first tested count where CUDA wins or explicitly state there was none. Overlapping ranges warrant caution; these three counts cannot establish an exact crossover.

Hardware data includes GPU name, selected ordinal and capability, installed NVIDIA driver version, CUDA driver API/runtime/header versions, per-trial build metadata, CPU/OS, binary SHA256, available compiler commands, source revision and dirty flag, correctness preflight output, trial order and configuration. The CUDA version integers use the runtime API convention (e.g. 12060 for 12.6); they are not the installed driver package version.

To exercise the full report workflow without a GPU:

```sh
cmake -S . -B build-m21-release -DCMAKE_BUILD_TYPE=Release -DCORESIM_BUILD_APP=OFF
cmake --build build-m21-release --parallel
python3 benchmarks/cuda_study.py --build build-m21-release --cpu-only \
  --output /tmp/coresim-m21-rehearsal
```

CPU-only rehearsal is explicit. It produces 30 trials by default, labels itself as a rehearsal, leaves all GPU phases null and does not calculate CUDA speedups. For a single diagnostic row (without the study's hardware preflight), use `coresim_particle_profile --backend serial|parallel|cuda` with the runner's count/seed/step options and `--warmup`.
