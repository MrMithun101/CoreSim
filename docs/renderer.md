# Rendering foundation

The renderer draws indexed cubes with six flat face colors; milestone 3 supplies 64 independent transforms. It uses an OpenGL 3.3 core context and GLSL 330 shaders. The camera and demo scene live in the CPU-only scene module; the renderer borrows a view-projection matrix and transform span. There is no entity/component model yet.

## Ownership and context lifetime

`Application` declares its owners in this order:

1. `GlfwRuntime`: process-wide GLFW initialization and error reporting.
2. `Window`: native window and OpenGL context; loads GLAD after making the context current.
3. `Renderer`: program, vertex buffer, index buffer, and vertex array.

Destruction reverses that order. Every GPU owner's construction, use, move assignment, and destruction requires the owning OpenGL context to be current on the main thread. The application maintains that invariant. The wrappers do not manage context switching or cross-thread resource sharing.

`VertexBuffer`, `IndexBuffer`, `VertexArray`, and `Shader` are noncopyable. Their noexcept move operations transfer handles and leave the source with handle zero. Move assignment first deletes the destination's old resource. Zero-handle deletion is harmless. Shader stages are temporary RAII objects; failed compilation or linking releases all objects constructed so far and reports the source paths with the driver log. GPU handles exposed by `id()` are borrowed and must not be deleted by callers.

## Draw path

The cube contains 24 vertices and 36 unsigned 32-bit indices. Vertices contain interleaved position and color (six floats). Each face has its own four vertices so adjacent faces do not interpolate each other's colors. The VAO captures the attribute layout and element-buffer binding. Index data is initially uploaded through `GL_ARRAY_BUFFER` to avoid accidentally modifying an unrelated VAO's element binding.

Each frame sets the viewport from actual framebuffer pixels, enables `GL_DEPTH_TEST` with `GL_LESS`, enables depth writes, and clears both color and depth. The CPU constructs `projection * view * model` with GLM and uploads a column-major matrix. One indexed triangle draw renders each cube using shared geometry. The program and VAO are unbound afterward. There is no per-frame mesh allocation or shader compilation. The renderer owns the relevant OpenGL state; it does not save and restore arbitrary external state.

The application advances each demo object with its own rotation speed and axis. Zero-size framebuffers skip rendering to avoid division by zero when minimized. Resizing recomputes aspect ratio and viewport each frame. GLM handles matrix math; camera movement is described in `scene.md`.

## Shaders and loader

`shaders/cube.vert` and `shaders/cube.frag` are copied into each build directory by CMake. The executable's default shader directory is an absolute build-time path, so launching from a different working directory works. Shader edits require rebuilding to update the copy, then restarting; there is no live reload. For relocated binaries or alternate shaders, pass `--shader-dir /path/to/shaders`. Missing files and compile/link errors terminate with a useful diagnostic and nonzero exit status.

The generated GLAD header is vendored under `third_party/glad`, with license and provenance. A single C translation unit instantiates its function pointers. Loading uses GLFW's address resolver after context creation, as described in the [GLFW context guide](https://www.glfw.org/docs/3.4/context_guide.html#context_glext). GLAD reports support for at least OpenGL 3.3 before the renderer is created. GLM 1.0.1 is fetched at a fixed commit or supplied through an installed CMake package.

## Verification

Display tests use hidden GLFW windows and an explicit RGBA8/depth24 framebuffer rather than relying on a hidden window's backing store. Readback confirms the cube writes color and depth, and different rotation angles produce different images. Two overlapping triangles test depth ordering: the near red triangle survives a farther blue triangle drawn later; disabling depth makes the same draw blue as a control. Tests also check move ownership, deletion of replaced handles, failed shader loading/compilation/linking, missing uniforms, zero-size drawing, and viewport updates.

The framebuffer fixture contains test-only OpenGL lifecycle code; production lifecycle calls are centralized in the renderer wrappers and window. Readback is synchronous and intended for correctness testing, not benchmarking. ASan/UBSan covers host code; it does not establish driver or GPU memory safety.
