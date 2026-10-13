# Angular dynamics

Milestone 13 extends the existing body integrator and sequential contact solver. `Transform::rotation` is the single orientation owner; `RigidBody::angular_velocity` is a world-space vector in radians/second. `add_torque` and `add_force_at_offset(force, world_offset_from_center)` accumulate one-tick loads. `clear_forces` clears both force and torque. Mass-zero and transform-less bodies do not integrate and clear their loads.

## Inertia and integration

`inertia(body, collider)` derives principal moments from the current mass and explicit collider dimensions: a solid sphere uses `2*m*r*r/5`; a box with half extents `(x,y,z)` uses `m*(y*y+z*z)/3` on its local x axis and analogous y/z values. Render scale has no physical effect. An unshaped body uses `m*identity`; a static body has zero inverse inertia. Deriving these values avoids stale caches after mass/shape edits. Dynamic planes, invalid dimensions, unrepresentable moments/inverses and invalid quaternions are rejected.

`world_tensor` rotates a diagonal tensor with `R * I_local * transpose(R)`. Angular integration reconstructs world momentum from orientation and velocity, adds torque times the fixed timestep, and estimates the next orientation using a midpoint angular velocity and a quaternion exponential. It normalizes the result and derives the new angular velocity from the same world momentum. This includes torque-free gyroscopic motion without treating an asymmetric body's angular velocity as constant. It is a finite-step numerical approximation, not an energy-preserving integrator. The ordinary free-top test bounds drift over 20 seconds at 120 Hz.

The equations for inertia transformation, angular momentum, torque and contact-point velocity follow David Baraff's [unconstrained rigid-body notes](https://www.cs.cmu.edu/~baraff/sigcourse/notesd1.pdf). The integration scheme and implementation here are our own; no reference code is copied.

## Geometry and response

Boxes now use oriented geometry. A 15-axis separating-axis test covers both face normals and edge cross-products. Face contacts clip an incident quadrilateral to reference-face side planes; edge contacts use closest points on support segments. Near-parallel edge axes are skipped and near-equal face depths are preferred to avoid unstable feature changes. The spatial hash uses rotation-expanded world AABBs. Static plane normals rotate with their transforms, and box/plane contacts use the box vertices. Sphere/sphere and sphere/plane remain supported; sphere/box and plane/plane remain unsupported.

Contact patches retain almost-touching box vertices within a 0.002-world-unit skin, with nonnegative penetration. This stabilizes resting faces. The velocity solver runs 16 iterations over individual contact points and uses point velocity `v + omega cross r`, rotational effective mass, and equal/opposite linear and angular impulses. A two-axis tangential solve projects accumulated friction impulses onto the Coulomb disk. Friction defaults to 0.5, is configurable in `[0,1]`, and combines as the geometric mean; set either body's friction to zero for a frictionless pair. Restitution retains the existing low-speed cutoff. Four translation-only penetration-correction passes process each contacting pair, not every manifold vertex. Correction is a geometric stabilization, not a momentum/energy-conserving physical impulse.

The normal impulse formulation is based on the contact equations in Baraff's [constrained rigid-body notes](https://www.cs.cmu.edu/~baraff/sigcourse/notesd2.pdf). Contact clipping, friction accumulation and solver implementation are local code. There is no warm starting, sleeping, CCD, rolling resistance or angular position correction; large timesteps, extreme velocities and deep/dense stacks are outside the demonstrated regime.

`CollisionStats::contacts` now counts contact points, while `correction_checks` counts pair retests. Historical benchmark numbers describe their recorded revisions, not this more expensive solver. Parallel integration keeps disjoint per-body ownership and joins before the serial contact solve; workers do not write shared solver or profiler state.

## Verification

Initial tests cover solid-body moments, rotated tensors, torque/offset-force response, quaternion normalization, free-top momentum and energy drift, static/missing bodies, invalid inputs, rotated box geometry, face patches, an analytical frictionless corner impulse, and off-center two-body momentum transfer. The existing resting-box and 100-body/two-body-stack regressions pass with the new solver. Final integration and sanitizer results will be recorded in `verification.md`.
