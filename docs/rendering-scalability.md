# Rendering scalability

Milestone 14 keeps the individual-draw renderer as a reference and adds OpenGL 3.3 instancing for repeated opaque cubes and spheres. `Renderer::draw` defaults to `RenderMode::instanced`; callers can select `RenderMode::individual`. `Renderer::stats()` reports submitted objects, draw calls and uploaded instance bytes for the last draw, including zero for a minimized framebuffer.

The renderer owns two reusable CPU matrix vectors. Every frame it joins meshes to transforms, computes the same view-projection/model product as the original path, and packs each mesh kind separately. Each Mesh owns its streaming vertex buffer and VAO through movable RAII owners. Four divisor-one vec4 attributes carry each column-major matrix. Nonempty batches issue one indexed instanced draw; empty batches issue none. Buffers retain their high-water capacity and orphan storage before uploading active data, letting the driver retire previous commands safely. CPU vectors also retain capacity. There is no transform cache, persistent mapping, culling, indirect draw, physics work or worker-thread GL access.

Batching changes mesh order. The current materials are opaque with depth testing; exactly equal-depth overlapping surfaces have order-dependent winners and are not promised identical results between modes. Transparent ordering would need a separate design. Framebuffer tests compare complete images for rotated/scaled mixed geometry and check growth, shrink, mutation, missing components, destroyed entities and empty batches. The test also alternates modes on the same renderer. Resource tests cover streamed-buffer move construction/assignment and replacement deletion.

## Reproduce

Build Release with `CORESIM_BUILD_APP=ON`. A desktop OpenGL context is required, including for the hidden benchmark window:

```sh
./build-release/coresim_render_benchmark --mode individual --objects 25000 --frames 30 --warmup 10
./build-release/coresim_render_benchmark --mode instanced --objects 25000 --frames 30 --warmup 10
python3 benchmarks/render.py --binary build-release/coresim_render_benchmark --output /tmp/render-study
```

The runner uses three trials at 1,000, 10,000 and 25,000 cubes by default, alternating mode order each trial. `--geometry mixed` alternates cubes and spheres. It retains every CSV row, GL identity log, command, return code, binary hash, source revision, dirty status and basic machine details. Each invocation warms up ten frames and records thirty. It validates counts, draw calls, positive timings, FPS reciprocity and exact final image checksums across both modes and every trial on the same device. An output directory must be new; failed invocations remain visible.

The deterministic orthographic grid is rendered to an explicit 1280×720 RGBA8/depth24 framebuffer. All objects lie in view; they are small at the largest counts. The cube has 12 triangles; the sphere has 768. Transforms and components are reconstructed for each process, outside timing. Matrix generation, component lookup, batch packing, buffer upload, clear and driver submission are inside CPU submission timing. Even unchanged matrices are uploaded each frame, matching the moving-body use case's data volume. Each sample begins with the preceding work completed and ends with `glFinish`; completed render frame time includes submission and completion waiting. CSV `fps` is 1000 divided by this offscreen render time, **not interactive application FPS**. No swap, VSync, physics, ImGui, event handling, initialization, readback or CSV writing is timed. No GPU timestamp query is used; submission can include driver work or stalls, and the difference between the two timings is not a pure GPU duration.

The pre-instancing baseline is retained in `benchmarks/results/render-baseline-m14` at benchmark commit `c965703`. Its draw-call field equals the known one-call-per-object implementation. The comparison executable reads actual renderer counters. This synthetic experiment isolates repeated-geometry rendering; it does not establish full physics-engine throughput or portability of a speedup to other GPUs.
