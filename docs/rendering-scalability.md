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

## Measured results — October 13, 2026

MacBookPro17,1, Apple M1 (8 CPU cores), 8 GiB RAM, macOS 26.3.1 arm64. OpenGL reports Apple M1, `4.1 Metal - 90.5`. Release, AppleClang 21.0.0.21000101, C++20. Comparison revision `2dfd015`, binary SHA-256 `e714c9daa7a27825066cabc46bd568e94e002eae7a1e26a9cfed5e3489cf0f19`. No builds or other CoreSim tests ran concurrently with measurements. System/desktop background activity was not controlled.

Each value below is the median of the three trial medians; brackets show the minimum–maximum **trial medians**, not a confidence interval. Every trial's raw extrema and all 1,080 measured comparison rows are retained, plus 270 original baseline rows. No samples or outliers were discarded.

| Geometry | Objects | Mode | Calls | CPU submission ms | Completed render ms | Offscreen FPS |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| cubes | 1,000 | individual | 1,000 | 0.946 [0.910–0.946] | 1.437 [1.403–1.474] | 695.703 [678.206–712.918] |
| cubes | 1,000 | instanced | 1 | 0.042 [0.042–0.045] | 0.412 [0.407–0.415] | 2425.107 [2409.525–2456.375] |
| cubes | 10,000 | individual | 10,000 | 9.119 [8.921–9.490] | 9.656 [9.472–10.096] | 103.557 [99.053–105.570] |
| cubes | 10,000 | instanced | 1 | 0.294 [0.292–0.308] | 0.981 [0.968–1.028] | 1019.780 [973.021–1032.613] |
| cubes | 25,000 | individual | 25,000 | 22.322 [22.014–22.732] | 22.629 [22.352–23.016] | 44.191 [43.451–44.739] |
| cubes | 25,000 | instanced | 1 | 0.749 [0.740–0.750] | 1.898 [1.864–1.981] | 526.856 [504.790–536.487] |
| mixed | 1,000 | individual | 1,000 | 3.182 [3.164–3.203] | 5.290 [5.232–5.304] | 189.022 [188.554–191.125] |
| mixed | 1,000 | instanced | 2 | 0.051 [0.048–0.075] | 0.803 [0.785–0.857] | 1244.781 [1167.260–1274.461] |
| mixed | 10,000 | individual | 10,000 | 29.297 [29.098–29.458] | 32.325 [32.028–32.328] | 30.936 [30.933–31.222] |
| mixed | 10,000 | instanced | 2 | 0.413 [0.411–0.413] | 2.970 [2.915–3.051] | 336.694 [327.736–343.043] |
| mixed | 25,000 | individual | 25,000 | 71.925 [71.865–72.276] | 73.896 [73.889–74.347] | 13.532 [13.450–13.534] |
| mixed | 25,000 | instanced | 2 | 0.768 [0.764–1.322] | 5.816 [5.805–6.314] | 171.937 [158.387–172.258] |

Raw data, all trial medians/extrema and invocation metadata: [original individual baseline](../benchmarks/results/render-baseline-m14), [paired cubes](../benchmarks/results/render-cubes-m14), [paired mixed meshes](../benchmarks/results/render-mixed-m14). Cube comparison started with a clean tree. The mixed run's dirty status lists only the preceding uncommitted cube result directory; the code and binary were unchanged.

The original 25,000-cube baseline spent 19.3–19.9 ms submitting and 19.6–20.2 ms completing each render (trial medians). That observation motivated reducing per-object driver calls. The same-binary comparison above confirms a large submission reduction while retaining matrix work and exact final images. Original and paired individual timings differ; they were measured at different times and the new mesh VAOs also include instance attributes. Claims therefore use the paired same-binary measurements, not the earlier baseline as a denominator.

Instancing reduces per-object uniform/draw calls, not triangle count. At 25,000 objects it uploads 1.6 MB of matrices each frame. Completed time remains substantially above submission time in the instanced cases, especially with spheres; geometry and driver/GPU completion are now a larger part of the workload. These timings alone cannot distinguish GPU execution from all driver synchronization costs. The mixed workload sends 9.75 million triangles at 25,000 objects versus 300,000 for cubes. Tiny projected triangles and a fixed framebuffer limit generalization to large close-up objects or expensive materials.

Both paths recompute matrices each frame; this experiment does not compare static-transform caching, culling, or partial uploads. Retained capacities avoid repeated CPU vector growth but driver orphaning can allocate internally. OpenGL errors are checked once per stream upload and per benchmark frame, outside the measured completion boundary for the latter check. The individual path remains selectable for comparison and diagnosis. The measured results support instancing as the demo default for its repeated opaque meshes; they are not a claim about full-scene physics FPS.
