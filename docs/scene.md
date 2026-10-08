# Camera and scene

`coresim_scene` contains CPU-only camera and transform math and links GLM. It has no GLFW or OpenGL dependency, so its tests run on headless CI.

`Camera` stores position, yaw, and pitch. Angles use radians. Movement uses local right/forward and world-up at 8 units/second, with diagonal movement normalized. Pitch is clamped to 89 degrees and yaw wraps to preserve a usable view basis. Projection uses a 60-degree vertical field of view and near/far distances of 0.1/200. Callers supply the framebuffer aspect ratio.

`Transform` stores position, a unit quaternion rotation, and scale. Model matrices use translation * rotation * scale. The caller maintains the unit-quaternion invariant. These are plain independent values; stable entity IDs and component storage belong to milestone 4.

## Input and demo

- W/S: forward/back along the view direction; A/D: strafe.
- Q/E: down/up along world Y.
- Hold the right mouse button: capture the cursor and look around. Release it to restore the cursor.
- Escape or the native close button: exit.

`Window` polls GLFW into a small `InputState` value. The application applies mouse deltas at 0.0025 radians per cursor unit, independent of frame duration, and movement at 8 units/second. Unfocused windows ignore input; capture transitions rebase the previous cursor position to avoid a first-frame jump. Cursor capture uses GLFW's disabled cursor mode, following the [GLFW input guide](https://www.glfw.org/docs/3.4/input_guide.html#cursor_mode).

Application motion and demo animation cap their timestep at 0.1 seconds after stalls. This is a responsiveness choice for the camera/demo, not a physics integrator or fixed timestep. Frame timing still records the actual elapsed time.

`DemoScene` owns a contiguous array of 64 independent transforms arranged in an 8×8 grid. Initial position, nonuniform scale, and rotation are deterministic. Each cube rotates with its own axis/speed; quaternion normalization prevents gradual length drift. There are no per-frame scene allocations.

The application owns the camera and scene and passes a view-projection matrix plus a read-only transform span to `Renderer`. The renderer owns only shared cube geometry and shaders. Each transform produces one uniform update and indexed draw. Instancing, entity IDs, component attachment, and physics remain future milestones.
