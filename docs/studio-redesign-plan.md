# Studio redesign: audit and delivery gates

CoreSim uses C++20, CMake, GLM, GLFW, Dear ImGui and OpenGL 3.3. The CPU Studio is independent of optional CUDA and HIP targets. Catch2 covers the headless world, dynamics, sessions and separate workloads; display tests exercise rendering and actual UI input. Existing benchmark binaries retain their own workloads and semantics.

World owns generation-checked entity IDs and packed components; pointers into pools cannot survive structural edits. Transform owns pose/render scale, while sphere radii and oriented box extents are explicit collider dimensions. Plane normals are world-space. Inertia derives from mass and collider shape. The application polls events, starts an ImGui frame, applies controls, advances bounded 120 Hz ticks (workers join inside the tick), then renders. Editor changes belong on that main-thread boundary. The original renderer batches MVP matrices by mesh, writes vertex colors and tests depth; it has no material, lighting or shadow pass. Camera currently supports WASD and captured right-drag look.

1. Interactive foundation: preserve legacy presets, add an authored sandbox and a separate build pose. Pick physical geometry using a camera ray. Commit transforms and body settings coherently; play/step/reset must not overwrite authored data. Test lifecycle, shape changes and restoration before moving on.
2. Presentation: add material-aware lighting and a coherent floor while retaining both renderer paths. Verify depth, instancing parity and framebuffer output. Extend navigation and shadows only with working controls and regression coverage.
3. Dynamics: measure settling and impact before changing solver behavior. Add controlled regressions; isolate any solver improvements from editor logic.
4. Structural Lab: build a resettable physical tower/projectile experiment with editable launch parameters. Demonstrate momentum transfer through existing contacts.
5. Finish persistence, history, shortcuts and diagnostics. Validate round trips and atomic failure handling, run the graphical suite, and document supported features and remaining work.

No commercial engine or GPU dependency will be introduced. GPU hardware validation remains independently pending.
