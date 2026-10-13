# Collision pipeline

Collision geometry belongs to `Collider`: sphere radius, oriented-box local half extents, or a static plane. Dimensions are explicit world units; transform position/rotation place the shape, and render scale is ignored. Constructors validate dimensions and plane normals; callers must preserve their invariants when editing fields. A rotated plane has world normal `rotation * normal` and solid half-space `dot(world_normal, point - position) <= offset`.

Supported pairs are sphere/sphere, oriented box/box, sphere/plane and box/plane, including reversed order. Sphere/box and plane/plane remain unsupported. Normals point from A toward B. `detect_manifold` returns a bounded contact patch; `detect_contact` returns its centroid, common normal and maximum penetration for geometric queries and pair correction. Coincident spheres use a deterministic X-axis normal. Box face clipping, edge contacts and the 0.002-unit resting-contact skin are described in [angular dynamics](angular-dynamics.md).

## Response

After linear/angular integration completes, a [spatial hash](spatial-hash.md) generates sorted, unique pairs. The bounded all-pairs reference remains available. Static/static pairs and missing transforms are skipped; missing rigid bodies behave as static colliders. Dynamic planes are rejected. The solver retains capacity but does not retain impulses across ticks.

Sixteen sequential-impulse iterations use contact-point velocity and rotational effective mass. Normal impulse accumulation stays nonnegative. Restitution combines as the minimum of both bodies, with restitution 1 for a missing body; closing speeds below 1 m/s use zero bounce. Two tangential directions use a coupled effective mass, with accumulated friction projected onto a Coulomb disk. Friction combines geometrically, defaults to 0.5 (including missing bodies), and is configurable from zero to one. Static linear/angular velocities are ignored.

Four position-correction iterations retest each detected pair and remove 80% of penetration beyond 0.001 units, weighted by inverse mass. They translate centers without changing velocities or orientations; this is stabilization, not physical momentum/energy transfer. New pairs introduced by correction wait until the next tick. `CollisionStats::contacts` counts manifold points; `correction_checks` counts pair retests. See the [angular equations and limitations](angular-dynamics.md).

There is no CCD, warm starting, sleeping, rolling resistance, angular position correction or guarantee for arbitrary tall/dense stacks. Large timesteps and fast objects can tunnel. These limitations are not addressed by adding orientation to the supported primitives.

## Demo and validation

The default scene has 50 spheres and 50 tilted/spinning boxes in two layers, with radius/half extents 0.65, restitution 0.25, friction 0.5 and varying mass. One infinite static plane supports both shapes. A visible static slab has no collider, avoiding duplicate support impulses. Shared cube/sphere GPU geometry is unchanged. Reset restores saved positions, orientations and linear/angular velocities and clears loads; stale IDs cannot affect replacement entities.

Tests retain the linear restitution/momentum and inverse-mass regressions, five seconds of 100 boxes on a box floor, and ten seconds of the mixed demo. New checks cover torque, inertia, gyroscopic motion, clipping, skew edges, rotated plane normals, rotated broad-phase bounds, analytical corner impact, angular momentum transfer, friction, and exact serial/parallel/naive/spatial state comparisons. These are bounded correctness workloads, not throughput claims.
