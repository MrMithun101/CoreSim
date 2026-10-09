# Collision V1

Collision geometry is an explicit `Collider` component: sphere radius, axis-aligned box half extents, or a static plane. Shape constructors validate positive finite dimensions and normalize plane normals. Callers must preserve those invariants if editing fields. Dimensions are world units and use only `Transform::position`; scale and rotation do not silently change colliders. In particular, boxes remain axis-aligned. The demo must match its visual scale/orientation to its collider.

`detect_contact` implements sphere/sphere, AABB/AABB, and sphere/plane (including reversed argument order). Touching counts as contact. Sphere/box, box/plane, and plane/plane are unsupported and return no contact. A plane is a solid half-space: `dot(normal, point - position) <= offset`; a sphere already below it is corrected toward the positive side. Plane offsets are distances along the normalized normal.

Contact normals point from A toward B. Position correction moves A opposite that normal and B along it. A contact contains both entity IDs, penetration, normal, and a representative contact point. Sphere coincidence uses a deterministic X-axis fallback. AABB detection chooses the minimum translation axis with deterministic ties and handles containment. There is one contact per pair; no angular manifold or friction is implemented.

The primitive tests cover separation, touching, overlap, containment, coincident centers, reversed arguments, translated planes, unsupported combinations, invalid shape construction, and collider lifecycle. Stable IDs and component cleanup follow the existing World rules.

## Response

After semi-implicit integration, the collision system scans all collider pairs, skipping static/static pairs and entities missing transforms. Missing rigid bodies behave as static colliders. Plane bodies must have zero mass. Contact storage retains capacity between steps; there is no spatial index, persistent contact cache, or warm starting.

Eight sequential-impulse iterations enforce a nonnegative accumulated normal impulse. The target separating speed is computed once from the initial closing speed and the minimum body restitution (a collider without a body contributes restitution 1). Closing speeds below 1 m/s use restitution zero to reduce resting jitter. Static-body velocities are ignored. Separating pairs receive no attractive impulse.

Four position-correction iterations recompute geometry for the detected pairs and remove 80% of penetration beyond 0.001 world units, weighted by inverse mass. Position correction does not change velocity. New pairs created by correction are considered on the next tick. The representative contact point is retained for future visualization; this solver applies only linear impulses.

This is discrete collision detection, so sufficiently fast bodies can tunnel through boxes/spheres. There is no friction, CCD, angular response, rotated-box support, or guarantee for tall stacks. The all-pairs baseline is intentionally unoptimized; Milestone 7 records its cost before spatial acceleration is added.

## Demo and validation scope

The default scene contains 100 dynamic bodies: 50 spheres on the left and 50 axis-aligned boxes on the right, arranged in two layers. All have radius/half extents 0.65, restitution 0.25, and varying mass. There are two static support entities: a visible box slab with its top at Y=0 for boxes and an invisible plane at Y=0 for spheres. Unsupported sphere/box and box/plane pairs mean these supports cannot apply two responses to the same body. The sphere plane is infinite; the visual slab and box support are finite.

Sphere meshes are generated once during renderer initialization with 16 latitude rings and 24 longitude sectors. Shared cube/sphere meshes reuse the existing RAII buffers, VAOs, and shader. Visual scales match collider dimensions, and demo boxes do not rotate. R restores the 100 original dynamic bodies; it does not recreate or reset the support entities.

Tests verify restitution at 0/0.5/1, equal-mass momentum, separating pairs, inverse-mass separation, static bodies with nonzero stored velocity, missing-body static colliders, plane restrictions, and ten seconds of sphere resting. A 100-AABB floor test runs five simulated seconds; the mixed 100-body demo runs ten seconds with two-body stacks. These are specific correctness/stability workloads, not throughput benchmarks or guarantees for arbitrary stacks. No friction, high-speed tunneling fix, or unsupported shape interaction is implied.

Milestone 7 retains this all-pairs algorithm with bounded 4096-pair batches and optional phase/counter instrumentation. See [benchmark definitions and measured baseline](../BENCHMARKS.md). Candidate counting includes all collider pairs; missing transforms and static/static pairs are filtered in the measured narrow phase.
