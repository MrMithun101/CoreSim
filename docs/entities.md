# Entity/component foundation

`World` is the single-threaded CPU owner of entities and their components. It has explicit APIs for the components used now: `Transform`, `MeshComponent`, and demo-only `SpinComponent`. There is no runtime type registry, inheritance tree, scheduler, or third-party ECS. Physics components will be introduced with their first consumers in later milestones.

## IDs and lifecycle

`Entity` contains a 32-bit slot index and 64-bit generation. IDs are local to the World that issued them; mixing IDs from different Worlds is a caller error and is not detected. World cannot be copied or moved. A default ID is invalid.

`create()` returns a live ID without automatically attaching components. `destroy()` removes every attached component and invalidates the ID. The slot may then be reused with a new generation. Destruction of an invalid/stale ID returns false. A generation at its maximum value retires the slot permanently rather than wrapping and reviving an old ID. The free list is embedded in slots, so destruction does not allocate.

`set_transform`, `set_mesh`, and `set_spin` attach or replace a value, throwing `std::invalid_argument` for a non-live ID. Lookups return pointers, or null for missing components/invalid IDs. Removal returns whether a component was actually removed; it does not destroy the entity. Component presence is independent, so systems must check the combinations they require.

## Storage and references

Each fixed component type uses a packed vector of `{Entity, value}` entries plus a sparse vector mapping slot index to packed index. Lookup is O(1); insertion is amortized O(1); removal swaps the final entry into the removed entry's slot and updates its sparse mapping. Iteration order is therefore not stable. Capacity is retained after removal, and sparse metadata scales with the highest allocated slot, not just live components.

Entity IDs remain stable across vector growth and compaction. Component pointers, references, and spans do not: conservatively treat any structural change to a pool as invalidating its views. Do not add/remove components or destroy entities during iteration; finish the loop first. Value-only updates do not invalidate views. Read-only entry spans protect entity bookkeeping from callers. Every pool verifies the full ID in addition to its sparse index.

The small internal `ComponentPool<T>` helper supports only the fixed, nonthrowing value-component assignments used by World; it is not a public general-purpose container. Insertion publishes its sparse index only after the dense append succeeds. Replacing values and swap removal perform no allocations. Adding a future component with throwing assignment would require revisiting those guarantees.

## Ownership boundary

Mesh components reference shared geometry through `MeshKind`; they never own OpenGL handles. GPU resource lifetime belongs to the renderer. World contains no OpenGL calls. The renderer and future physics systems must borrow the CPU data they need rather than own each other's state.

## Tests

Tests cover stale/default IDs, generation changes after reuse, repeated destruction, all-component cleanup, missing/replaced/removed components, compaction, const lookups, and a fixed-seed 6,000-operation reference-model comparison. Generation exhaustion is guarded in code but is not iterated to exhaustion in tests.

## Demo and rendering integration

`DemoScene` owns a World and populates 64 entities with transform, cube-mesh reference, and spin components. Spin axes/speeds are attached once and travel with their entity; compaction cannot silently change animation identity. The animation loop iterates spin entries, looks up each transform, and updates only matching entities. Removing a transform or omitting spin is valid.

`Renderer::draw` borrows a const World for the duration of the call. It iterates mesh entries, looks up transforms by ID, and draws the supported intersection. Neither the renderer nor the demo retains component pointers across structural changes. A transform without a mesh is invisible; a mesh without a transform is skipped; an entity without spin is static. Destruction removes it from rendering immediately, and reusing the slot starts with no components.

Display tests use actual framebuffer readback to check those combinations, including restoring a removed transform and reusing a destroyed entity's slot. The scene remains visually equivalent to milestone 3 at initialization. The title reads the actual live entity count rather than a hardcoded 64. Future rigid-body and collider components remain unimplemented, as do physics integration and collision behavior.
