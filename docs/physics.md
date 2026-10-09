# Translational physics V1

`World` owns `RigidBody` components independently from transforms and mesh references. A body's position is the existing `Transform::position`; there is no duplicate position to synchronize. `PhysicsSystem` borrows World and never touches graphics resources. The renderer does not own physics state. Component value methods are compiled with the scene data; the physics algorithm is a separate `coresim_physics` library to avoid a cyclic scene/physics dependency.

## State and integration

Units are meters, seconds, kilograms, and newtons. Each body stores velocity, persistent user acceleration, a one-tick force accumulator, mass/inverse mass, and restitution. Setters keep mass and inverse mass consistent. Mass zero denotes a static body; it is not integrated, even if velocity is nonzero. Restitution is validated in [0,1] and used by collision response, with a low-speed bounce cutoff.

For dynamic entities with transforms, a physics tick computes:

```
a = gravity + user_acceleration + accumulated_force * inverse_mass
v = v + a * h
position = position + v * h
```

This is semi-implicit Euler. Gravity defaults to `(0, -9.81, 0)` and is independent of mass. Velocity and position use floats. The collision solver follows integration; see `collisions.md`. There is no damping, sleeping, or angular physics. Demo quaternion animation is separate from physical angular dynamics.

Forces accumulate until the next physics tick and are then cleared, including for static bodies and entities missing transforms. No tick means no clearing. A continuous force must be applied inside the fixed-tick callback before every step; adding it once per render frame would make force duration depend on render FPS. Persistent user acceleration is not cleared.

Validated setters reject nonfinite force/mass/restitution values and negative mass. The integrator rejects invalid timesteps and nonfinite calculated velocity/position. Physics failure is fatal in the application; a multi-body step is not transactional and earlier bodies may already have advanced if a later body's state is invalid. Callers must maintain finite position, velocity, and user acceleration in ordinary operation.

## Fixed clock and overload behavior

`FixedStepper` accumulates double-precision render durations and invokes its callback with the same float representation of **1/120 second** every tick. It retains a sub-tick remainder. A tiny relative tolerance compensates for roundoff at tick boundaries. Rendering remains variable-rate and uses the latest physics state; interpolation is not implemented yet.

Each frame accepts at most 0.25 seconds and executes at most 16 ticks. Whole ticks still owed after this budget are discarded, retaining only the fractional remainder. `StepResult::dropped_seconds` includes both the frame-time clamp and discarded backlog. This bounds catch-up cost after a stall but intentionally slows simulation relative to wall time under sustained overload. No claim of frame-rate independence applies when time is being discarded.

`reset()` clears the accumulator. The callback should not throw during normal operation; failures propagate to the application. There is no threading or optimization in this baseline.

## Tests

The CPU suite checks semi-implicit integration against its known discrete solution, gravity independence from mass, inverse-mass force response, static/missing-transform handling, force accumulation and clearing, pending forces across no-tick frames, component lifecycle, invalid inputs, and overload accounting. Two seconds at 30, 60, and 144 render FPS produce identical 240-tick results for the tested workload. This is a correctness check, not a performance benchmark or a cross-platform bitwise-determinism claim.

## Demo and reset

The default collision scene has 100 dynamic bodies (50 spheres and 50 axis-aligned boxes) above two static floor supports. Bodies begin at rest in two layers, fall, bounce, and settle. Orientations are fixed; demo spin remains available as a separate component but is not attached in this scene. Details and unsupported shape combinations are documented in `collisions.md`.

Press **R** to restore the original entities' positions/velocities, clear their forces and user acceleration, and reset the accumulator, tick counter, and dropped-time counter. Reset uses generation-checked saved IDs and cannot modify replacement entities that reuse destroyed slots. It does not restore removed components, mass/restitution edits, or demo orientation. Holding R triggers only one reset until it is released.

The application feeds actual frame elapsed time to FixedStepper (not the camera's 0.1-second movement clamp). Physics and the optional demo-spin system update inside the fixed callback. The window title shows simulated seconds and dropped time; shutdown reports tick count and dropped seconds. No renderer changes are required to observe physical motion: it reads updated transforms from World. Rendering uses the latest state without interpolation, so slight 120 Hz stepping may be visible on high-refresh displays.

Additional tests cover irregular frame durations, the 100-dynamic/2-static demo composition, gravity-driven motion relative to static references, reset and stale-ID safety, and framebuffer changes after real fixed physics ticks.
