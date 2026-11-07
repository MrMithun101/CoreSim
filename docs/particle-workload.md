# Comparable particle workload — Milestone 18

`coresim_particle_runner` establishes a common CPU workload for later CUDA/HIP comparisons. It provides serial execution and persistent worker-pool execution, deterministic initial state, a fixed timestep, and full-state correctness checks. It runs headlessly without OpenGL, HIP or CUDA. This milestone makes no performance or GPU execution claim.

## Build and run

```sh
cmake -S . -B build-m18-release -DCMAKE_BUILD_TYPE=Release -DCORESIM_BUILD_APP=OFF
cmake --build build-m18-release --parallel
ctest --test-dir build-m18-release --output-on-failure
./build-m18-release/coresim_particle_runner --particles 1000
./build-m18-release/coresim_particle_runner --particles 10000 --workers 4
./build-m18-release/coresim_particle_runner --particles 100000 --steps 120 --workers 8 --seed 7
```

The default backend is `compare`: run both paths from identical inputs and compare every position and velocity component after **every tick**. A mismatch or execution failure exits nonzero. No timer is included in this correctness runner.

```sh
./build-m18-release/coresim_particle_runner --backend serial --particles 100000 --steps 120 --seed 7
./build-m18-release/coresim_particle_runner --backend parallel --particles 100000 --steps 120 --workers 4 --seed 7
```

Independent serial/parallel runs report `cpu_parity=not-checked`; successful execution alone is not a parity test. Comparison runs report `cpu_parity=exact-every-tick`. Output includes workload version, backend, count, seed, ticks, actual float timestep/acceleration, pool worker count, submitted worker tasks and a diagnostic state digest. A worker-task count is not a utilization measurement or proof of simultaneous execution.

CLI bounds: 1–100,000 particles, 1–10,000 steps, 1–64 workers, uint32 seed including zero. Defaults: 1,000 particles, 120 steps, four workers, seed 1. Duplicate, unknown, missing, negative, out-of-range and partially parsed arguments fail. `--workers` is rejected for serial mode; serial runs create no pool and report zero workers/tasks. Milestone 20 adds an [optional CUDA particle backend](cuda-particles.md); HIP remains a separate vector-add workload. The library additionally supports empty state for boundary testing.

## Version 1 contract

Each particle consists of six IEEE-754 float32 scalars in a contiguous 24-byte, standard-layout record: `x, y, z, vx, vy, vz`. The record has no entity ID, render transform, mass, collider, orientation or hidden padding. This array-of-structures representation is the explicit baseline; it is not claimed to be the optimal GPU or CPU layout.

Initial conditions use uint32 arithmetic:

```
state = state * 1664525 + 1013904223  (mod 2^32)
u = float(state >> 8) / 16777216
```

For each particle, six draws occur in order: x, y, z, vx, vy, vz. Positions are `16*u - 8` in [-8,8); velocities are `2*u - 1` in [-1,1). No standard-library random distribution is involved. Generation happens on the host and precedes execution. The same seed yields the same prefix when particle count changes.

The fixed timestep is float32 `1.0F/120.0F` (approximately 0.00833333377 seconds). Acceleration is constant and defaults to `(0, -9.81F, 0)`; the library accepts other finite acceleration vectors for analytical tests. The CLI keeps these rules fixed. For each axis, semi-implicit Euler performs:

```
v_next = v + acceleration * dt
position_next = position + v_next * dt
```

Particles are independent. There are no boundary conditions, wrapping, particle collisions, gravity between particles or rendering passes. Positions may leave the initialization volume. The older Studio Particle Storm uses different rules (zero gravity and periodic wrapping), so it must not be used as an equivalent benchmark. Studio integration is deferred to Milestone 23.

## Execution, ownership and failure

`ParticleSimulation` owns initial, current and scratch arrays. Reset copies the original state and clears the tick count; it does not regenerate from a different seed. Serial and parallel paths call the same scalar update with identical arithmetic order. A parallel tick submits `min(particle_count, worker_count)` jobs with balanced, contiguous, disjoint index ranges, reads only current state, and writes only scratch state. The runner constructs its pool once per run, not per tick. Even a one-worker parallel run uses the pool; it is not silently relabeled serial execution.

The coordinator waits for all submitted work before swapping current and scratch arrays and incrementing the tick count. Nonfinite output, worker exceptions and failed admission leave the published state and tick count unchanged. Already accepted jobs are drained before stack references can expire. Scratch contents after a failed tick are unspecified and are fully overwritten by the next successful tick. The simulator has no asynchronous API; stepping/reset from a pool worker is rejected to avoid nested waiting. Callers must provide exclusive access while stepping or resetting, including against reads/copies. State spans must not be retained across steps/reset.

The three owned arrays consume 72 bytes per particle in element payload: 7.2 MB (decimal) at 100K, excluding vector bookkeeping, allocator overhead, thread stacks and temporary initialization storage. Comparison mode owns two simulations. This is a layout-derived storage description, not a measured memory profile.

## Correctness evidence and limits

Tests cover fixed initialization bit patterns, seed changes, range limits, empty state, uneven partitions, counts smaller than the worker pool, 1K/10K/100K states, reset/replay, exact per-tick CPU parity, rejection of nonfinite inputs, numerical overflow, closed-pool submission failure and nested-worker calls. An independent double-precision analytical check uses the semi-implicit Euler closed form:

```
v_n = v_0 + n*a*dt
p_n = p_0 + n*v_0*dt + a*dt*dt*n*(n+1)/2
```

It allows 0.0002 absolute error for the bounded 120-tick float32 test. CPU parity itself requires exact finite scalar equality within the same binary. This does not promise cross-compiler/architecture bitwise trajectories or define a future GPU tolerance. GPU comparison will need an explicit numerical error policy and real hardware validation.

The digest hashes scalar IEEE bit patterns in a fixed low-byte-first order. It avoids structure padding and is useful for replay diagnostics, but it is not the correctness oracle. Full comparison always checks all six components and treats matching NaNs/infinities as invalid. Signed zeros compare equal numerically but have distinct digest bits.

[Retained local validation runs](../benchmarks/results/particles-m18/verification.txt) cover 1K, 10K and 100K particles for 120 ticks with 1, 2, 4 and 8 workers, seed 7. They include binary hash and host metadata. These results contain no speedup measurements. Full build/test evidence is in [verification notes](verification.md). Milestone 19 adds a separate [CPU timing and memory benchmark](particle-cpu-baseline.md).
