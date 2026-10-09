# Paper-strategy study — comparative results

All timing values are best-of-N seconds. `loc` = locality-aware scheduler (work-dealing +
vicinity work-stealing), `std` = standard scheduler (creator-queue + global stealing like
the work-stealing baseline). `coarse`/`fine` = data distribution policy passed to `ps_alloc`.

- **map** identical checksums across all configs: True
- **vecmul** identical checksums across all configs: True
- **reduction** identical checksums across all configs: True
- **matmul** identical checksums across all configs: True

Best-of-N times (s), normalized to plain OpenMP:

| benchmark | omp | std/coarse | loc/coarse | loc/coarse/v1 | loc/coarse/v2 | loc/fine |
|---|---|---|---|---|---|---|---|
| map | 0.0404 (1.00x) | 0.0361 (0.90x) | 0.0389 (0.96x) | 0.0383 (0.95x) | 0.0353 (0.88x) | 0.0351 (0.87x) |
| vecmul | 0.1764 (1.00x) | 0.1542 (0.87x) | 0.1685 (0.96x) | 0.1680 (0.95x) | 0.1640 (0.93x) | 0.1696 (0.96x) |
| reduction | 0.1712 (1.00x) | 0.1879 (1.10x) | 0.1811 (1.06x) | 0.2422 (1.41x) | 0.2060 (1.20x) | 0.1769 (1.03x) |
| matmul | 0.0474 (1.00x) | 0.0391 (0.82x) | 0.0407 (0.86x) | 0.1020 (2.15x) | 0.0635 (1.34x) | 0.0419 (0.88x) |

Plot: `strategy.png`. Raw data: `strategy.csv`, scheduler stats: `stats.log`.
