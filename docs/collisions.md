# Collision V1

Collision geometry is an explicit `Collider` component: sphere radius, axis-aligned box half extents, or a static plane. Shape constructors validate positive finite dimensions and normalize plane normals. Callers must preserve those invariants if editing fields. Dimensions are world units and use only `Transform::position`; scale and rotation do not silently change colliders. In particular, boxes remain axis-aligned. The demo must match its visual scale/orientation to its collider.

`detect_contact` implements sphere/sphere, AABB/AABB, and sphere/plane (including reversed argument order). Touching counts as contact. Sphere/box, box/plane, and plane/plane are unsupported and return no contact. A plane is a solid half-space: `dot(normal, point - position) <= offset`; a sphere already below it is corrected toward the positive side. Plane offsets are distances along the normalized normal.

Contact normals point from A toward B. Position correction moves A opposite that normal and B along it. A contact contains both entity IDs, penetration, normal, and a representative contact point. Sphere coincidence uses a deterministic X-axis fallback. AABB detection chooses the minimum translation axis with deterministic ties and handles containment. There is one contact per pair; no angular manifold or friction is implemented.

The primitive tests cover separation, touching, overlap, containment, coincident centers, reversed arguments, translated planes, unsupported combinations, invalid shape construction, and collider lifecycle. Stable IDs and component cleanup follow the existing World rules.
