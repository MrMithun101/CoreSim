# Reproducible scenes

Milestone 15 adds named CPU scenes to `DemoScene` while retaining the original 100-body collision demo as the default. The scene owns its World, initial motion snapshots, gravity and starting camera. Application supplies the existing fixed 120 Hz physics step and reusable job pool; Renderer reads the same mesh/transform components through instancing. No HIP code or GPU simulation is introduced.

| CLI name | Default objects | Accepted objects | Behavior |
| --- | ---: | ---: | --- |
| `collision-demo` | 100 | 100 | Existing separated sphere/box demo |
| `stress-tower` | 256 | 1–1,000 | Stacked unit cubes, low restitution and friction; top cube starts tilted and spinning |
| `sphere-rain` | 2,500 | 1–25,000 | Jittered lattice of radius-0.3 spheres falls onto a plane |
| `particle-storm` | 25,000 | 1–100,000 | Independent moving particles, zero gravity, periodic volume, no colliders |

Counts describe dynamic objects; tower and rain add a visible floor and a collision plane. Count limits bound input, not runtime or frame-rate guarantees. Large contacting scenes can exceed the real-time fixed-step budget; the window title reports dropped simulation time. Stress Tower stresses the existing solver and can settle or tumble, especially with larger/deeper stacks. It is not a structural stability guarantee.

```sh
./build-release/coresim --scene stress-tower
./build-release/coresim --scene sphere-rain --objects 25000 --seed 7
./build-release/coresim --scene particle-storm --objects 25000 --seed 7
```

WASD/QE and right-mouse look retain their existing behavior. Each named scene starts with a camera aimed at its initial volume. R restores saved position, quaternion, linear/angular velocity, clears applied loads and acceleration, and resets simulation time. Camera navigation is retained. Reset operates on surviving bodies/transforms; it does not recreate deleted entities or undo user changes to shapes/materials. `--frames N` bounds display smoke tests, but elapsed wall-clock scheduling means it does not specify an exact number of physics ticks.

## Construction and particle boundaries

The lattice side is the smallest integer whose cube contains the requested count, filled x, then z, then y. Tower spacing is 1.02 with half-extents 0.5; its last cube starts 0.3 higher with a 0.12-radian tilt and nonzero spin. Rain spacing is 1.4, with the bottom layer centered at 3.3 and componentwise jitter of at most 0.075. The initial velocities and particle positions use a fixed 32-bit LCG with multiplier 1664525 and increment 1013904223, converting the upper 24 bits to a float. Random draws have explicit evaluation order. Default seed is 1; zero through UINT32_MAX are accepted. The original collision demo does not consume the seed.

Particles use radius-like visual half-extents 0.045, lattice spacing 0.35, and velocities in [-2,2) per axis. They are currently collider-free RigidBody/Transform entities rendered as tiny cubes, reusing the tested CPU integration path. After physics joins, a serial boundary pass wraps positions into a centered periodic volume; opposite boundaries identify the same location. Particles therefore remain visible rather than falling forever. They neither collide nor use lifetime/respawn logic. Integration can run on existing workers; boundary wrapping is serial. This supplies a reproducible independent workload for later CPU/GPU comparisons, not an optimized packed particle backend or a new parallel speedup claim.

## Exact-tick headless runner

`coresim_scene_runner` is available with `CORESIM_BUILD_APP=OFF` and requires no GLFW or GPU:

```sh
./build-release/coresim_scene_runner --scene stress-tower --objects 256 --seed 1 --steps 1200 --workers 4 > tower.csv
./build-release/coresim_scene_runner --scene particle-storm --objects 25000 --seed 7 --steps 1200 --workers 0 > particles.csv
```

`--steps` accepts 1–10000, default 120. `--workers 0` means serial; 1–8 creates that many reusable workers, subject to the existing integration grain threshold. Row zero describes initial motion with zero collision counters; subsequent rows follow exact 1/120-second ticks. Schema 1 includes scene/count/seed/workers, tick, simulated seconds, candidate pairs, contact points, minimum dynamic center height, maximum linear speed and a 64-bit FNV-1a motion digest. The digest mixes float bits of positions, linear/angular velocities and quaternions in dynamic component order, least-significant byte first. It is a diagnostic, not a proof of state equality or a cross-platform physics guarantee. Timing is deliberately absent; this runner verifies trajectories, not performance. Nonfinite motion fails with a nonzero exit status. Duplicate, unknown, malformed and out-of-range options fail before scene allocation.

Fixed seeds and ticks allow repeatable runs on the same build/platform. Floating-point collision solving is not guaranteed bitwise identical across compilers or architectures. Unit tests compare complete motion state between repeated setups, after reset and after serial/worker integration. Local longer-run observations and limits are recorded in [verification](verification.md).
