# CPU-only rehearsal — no GPU measurements

arm64 (model unavailable); 5 trials, 120 ticks per timed batch, 60 warmup ticks, seed 7.

Times are milliseconds per batch. Device intervals are medians; dashes mean not applicable.

| Particles | Backend | End-to-end median (min–max) | Upload | Kernel batch | Download | Max abs error |
|---:|---|---:|---:|---:|---:|---:|
| 1,000 | serial | 0.20254 (0.19783–0.22104) | — | — | — | 0 |
| 1,000 | parallel | 1.3762 (1.2989–1.5406) | — | — | — | 0 |
| 10,000 | serial | 1.9731 (1.9586–2.0506) | — | — | — | 0 |
| 10,000 | parallel | 2.6599 (2.6008–2.7877) | — | — | — | 0 |
| 100,000 | serial | 20.133 (19.896–20.894) | — | — | — | 0 |
| 100,000 | parallel | 11.002 (10.497–11.182) | — | — | — | 0 |
