# Camera and scene

`coresim_scene` contains CPU-only camera and transform math and links GLM. It has no GLFW or OpenGL dependency, so its tests run on headless CI.

`Camera` stores position, yaw, and pitch. Angles use radians. Movement uses local right/forward and world-up at 8 units/second, with diagonal movement normalized. Pitch is clamped to 89 degrees and yaw wraps to preserve a usable view basis. Projection uses a 60-degree vertical field of view and near/far distances of 0.1/200. Callers supply the framebuffer aspect ratio.

`Transform` stores position, a unit quaternion rotation, and scale. Model matrices use translation * rotation * scale. The caller maintains the unit-quaternion invariant. Transform values now live in World component storage and are retrieved by generation-checked entity IDs. See `entities.md` for lifecycle and invalidation rules.

## Input and demo

- W/S: forward/back along the view direction; A/D: strafe.
- Q/E: down/up along world Y.
- Hold the right mouse button: capture the cursor and look around. Release it to restore the cursor.
- R: reset original demo body motion and the physics clock.
- Escape or the native close button: exit.

`Window` polls GLFW into a small `InputState` value. The application applies mouse deltas at 0.0025 radians per cursor unit, independent of frame duration, and movement at 8 units/second. Unfocused windows ignore input; capture transitions rebase the previous cursor position to avoid a first-frame jump. Cursor capture uses GLFW's disabled cursor mode, following the [GLFW input guide](https://www.glfw.org/docs/3.4/input_guide.html#cursor_mode).

Camera motion caps its timestep at 0.1 seconds after stalls. This clamp now applies only to the camera; physics and demo spin use the separate fixed clock described in `physics.md`. Frame timing still records the actual elapsed time.

`DemoScene` owns 100 dynamic entities (50 spheres and 50 boxes) and two static floor supports. Initial transforms are deterministic and collider dimensions match visual scales. Demo spin is still available for nonphysical animation but is not attached in the collision demo; axis-aligned boxes must keep their visual orientation fixed. There are no per-frame scene allocations; collision contact storage may grow when a new high-water mark is reached.

The application owns the camera and scene and passes a view-projection matrix plus a const World reference to `Renderer`. The renderer owns shared cube/sphere geometry and shaders. Each entity with both a supported mesh and transform produces one uniform update and indexed draw. Instancing and angular dynamics remain future milestones. Translational physics now runs at 120 Hz. R resets the original demo bodies without recreating entities; bodies now hit the supported floor geometry.
