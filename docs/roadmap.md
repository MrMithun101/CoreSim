# Current CoreSim roadmap

Revised November 6, 2026: prioritize a benchmarkable CPU/GPU simulation engine, using Studio as its interface. Engine Milestones 1–16 and Studio shell Milestone 17 are retained.

| Milestone | Scope |
|---|---|
| 18 | Comparable particle workload: fixed rules, seeded inputs, serial/worker-pool CPU paths and correctness checks at 1K, 10K and 100K particles. |
| 19 | CPU measurement baseline: reproducible trials, relevant layout comparisons, measured memory usage, machine/build metadata and result export. |
| 20 | Optional CUDA particle integration implementing the same workload; CPU-only builds remain portable and the HIP foundation stays available. |
| 21 | Real NVIDIA GPU validation/profiling: correctness policy, kernel/transfer/end-to-end times and hardware-backed results. |
| 22 | Particle collision processing: boundaries first, then spatially partitioned particle–particle collisions, validated before timing. |
| 23 | Comparative report and supported execution controls in Studio; extend the equivalent workload to HIP when AMD hardware is available. |

The previous Studio V1 live configuration, algorithm switches, debug visualization and dashboard work is deferred. It is not silently included in the revised Milestone 18. Accounts, cloud storage and community features remain outside the immediate scope.

GPU correctness/performance cannot be established by compilation. No compatible GPU machine is currently available to the project owner. GPU milestones requiring execution remain conditional on hardware access. The existing Apple Silicon CPU build provides portable ARM execution; CUDA is optional, not a prerequisite for the CPU application.
