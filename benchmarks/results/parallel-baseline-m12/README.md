# Pre-change baseline

Measured October 12, 2026 at `a855ddb`, before parallel integration changes. Apple M1, 8 CPU cores, 8 GiB RAM, macOS 26.3.1 (25D771280a), Apple Clang 21, Release. No concurrent build/test jobs. Commands:

```sh
./build-release/coresim_layout_benchmark --layout lookup --bodies 1000000 --steps 30 --warmup 10
./build-release/coresim_layout_benchmark --layout physics-lookup --bodies 10000 --steps 30 --warmup 10
```

Raw CSV is preserved here. Medians: 9.882313 ms isolated integration (one million bodies); 2.632834 ms full sparse-collision physics (10,000 bodies). These short baseline observations establish the cost of integration and motivated the independent-range experiment. Formal paired scaling comparisons use the retained serial path in the new benchmark at the same revision as the parallel path, with more trials and balanced run order; see `docs/parallel-physics.md` from the repository root.
