# Translational physics V1

`World` owns `RigidBody` components independently from transforms and mesh references. A body's position is the existing `Transform::position`; there is no duplicate position to synchronize. `PhysicsSystem` borrows World and never touches graphics resources. The renderer does not own physics state. Component value methods are compiled with the scene data; the physics algorithm is a separate `coresim_physics` library to avoid a cyclic scene/physics dependency.

## State and integration

Units are meters, seconds, kilograms, and newtons. Each body stores velocity, persistent user acceleration, a one-tick force accumulator, mass/inverse mass, and restitution. Setters keep mass and inverse mass consistent. Mass zero denotes a static body; it is not integrated, even if velocity is nonzero. Restitution is validated in [0,1] but is not used until collision response exists.

For dynamic entities with transforms, a physics tick computes:

```
a = gravity + user_acceleration + accumulated_force * inverse_mass
v = v + a * h
position = position + v * h
```

This is semi-implicit Euler. Gravity defaults to `(0, -9.81, 0)` and is independent of mass. Velocity and position use floats. There is no damping, collision, floor, sleeping, or angular physics. Demo quaternion animation is separate from physical angular dynamics.

Forces accumulate until the next physics tick and are then cleared, including for static bodies and entities missing transforms. No tick means no clearing. A continuous force must be applied inside the fixed-tick callback before every step; adding it once per render frame would make force duration depend on render FPS. Persistent user acceleration is not cleared.

Validated setters reject nonfinite force/mass/restitution values and negative mass. The integrator rejects invalid timesteps and nonfinite calculated velocity/position. Physics failure is fatal in the application; a multi-body step is not transactional and earlier bodies may already have advanced if a later body's state is invalid. Callers must maintain finite position, velocity, and user acceleration in ordinary operation.

## Fixed clock and overload behavior

`FixedStepper` accumulates double-precision render durations and invokes its callback with the same float representation of **1/120 second** every tick. It retains a sub-tick remainder. A tiny relative tolerance compensates for roundoff at tick boundaries. Rendering remains variable-rate and uses the latest physics state; interpolation is not implemented yet.

Each frame accepts at most 0.25 seconds and executes at most 16 ticks. Whole ticks still owed after this budget are discarded, retaining only the fractional remainder. `StepResult::dropped_seconds` includes both the frame-time clamp and discarded backlog. This bounds catch-up cost after a stall but intentionally slows simulation relative to wall time under sustained overload. No claim of frame-rate independence applies when time is being discarded.

`reset()` clears the accumulator. The callback should not throw during normal operation; failures propagate to the application. There is no threading or optimization in this baseline.

## Tests

The CPU suite checks semi-implicit integration against its known discrete solution, gravity independence from mass, inverse-mass force response, static/missing-transform handling, force accumulation and clearing, pending forces across no-tick frames, component lifecycle, invalid inputs, and overload accounting. Two seconds at 30, 60, and 144 render FPS produce identical 240-tick results for the tested workload. This is a correctness check, not a performance benchmark or a cross-platform bitwise-determinism claim.
