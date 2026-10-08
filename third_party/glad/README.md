# GLAD loader

`include/glad/gl.h` is copied unchanged from GLFW 3.4, commit
`7b6aead9fb88b3623e3b3725ebb42670cbe4c579`, path `deps/glad/gl.h`.
It is a generated GLAD 2.0.0-beta single-header OpenGL 3.3 compatibility loader.
CoreSim requests a core context and uses only core functions. GLFW resolves
entry points; GLAD does not load platform libraries itself. Keeping generated
code here avoids requiring Python or a generator during normal builds.

The generator options and Khronos notice are embedded in the header.
`LICENSE` is the GLAD upstream license from tag v2.0.0.
`gl.c` instantiates the header implementation once.
