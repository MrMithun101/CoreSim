# Camera and scene

`coresim_scene` contains CPU-only camera and transform math and links GLM. It has no GLFW or OpenGL dependency, so its tests run on headless CI.

`Camera` stores position, yaw, and pitch. Angles use radians. Movement uses local right/forward and world-up at 8 units/second, with diagonal movement normalized. Pitch is clamped to 89 degrees and yaw wraps to preserve a usable view basis. Projection uses a 60-degree vertical field of view and near/far distances of 0.1/200. Callers supply the framebuffer aspect ratio.

`Transform` stores position, a unit quaternion rotation, and scale. Model matrices use translation * rotation * scale. The caller maintains the unit-quaternion invariant. These are plain independent values; stable entity IDs and component storage belong to milestone 4.
