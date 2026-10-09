# Timing summary

- matmul: checksum relative difference = 0.000e+00 (OK)
- heat2d: checksum relative difference = 0.000e+00 (OK)
- kmeans: labelhash identical: True
- map: checksum relative difference = 0.000e+00 (OK)
- reduction: checksum relative difference = 0.000e+00 (OK)
- sparselu: checksum relative difference = 0.000e+00 (OK)
- vecmul: checksum relative difference = 0.000e+00 (OK)

Best-of-N times in seconds (N=reps per config, size per kernel):

| threads | matmul | heat2d | kmeans | map | reduction | sparselu | vecmul |
|---|---|---|---|---|---|---|---|
| seq | 1.7306 | 1.4973 | 1.4234 | 0.3557 | 0.3381 | 0.4615 | 0.2252 |
| 1 | 1.8585 | 1.5404 | 1.5939 | 0.3615 | 0.3527 | 0.4669 | 0.2319 |
| 2 | 0.9315 | 1.0117 | 0.9921 | 0.2441 | 0.2472 | 0.2450 | 0.1127 |
| 4 | 0.4694 | 0.7321 | 0.5786 | 0.1985 | 0.1769 | 0.1379 | 0.0690 |
| 8 | 0.3769 | 0.7508 | 0.4541 | 0.2081 | 0.1786 | 0.1643 | 0.0753 |
| 12 | 0.3697 | 0.7112 | 0.3861 | 0.1992 | 0.1704 | 0.1864 | 0.1368 |
| 16 | 0.3231 | 0.7495 | 0.3327 | 0.1993 | 0.1749 | 0.1571 | 0.1899 |

Speedup vs sequential:

| threads | matmul | heat2d | kmeans | map | reduction | sparselu | vecmul |
|---|---|---|---|---|---|---|---|
| 1 | 0.93x | 0.97x | 0.89x | 0.98x | 0.96x | 0.99x | 0.97x |
| 2 | 1.86x | 1.48x | 1.43x | 1.46x | 1.37x | 1.88x | 2.00x |
| 4 | 3.69x | 2.05x | 2.46x | 1.79x | 1.91x | 3.35x | 3.26x |
| 8 | 4.59x | 1.99x | 3.13x | 1.71x | 1.89x | 2.81x | 2.99x |
| 12 | 4.68x | 2.11x | 3.69x | 1.79x | 1.98x | 2.48x | 1.65x |
| 16 | 5.36x | 2.00x | 4.28x | 1.78x | 1.93x | 2.94x | 1.19x |

Parallel efficiency (speedup / threads):

| threads | matmul | heat2d | kmeans | map | reduction | sparselu | vecmul |
|---|---|---|---|---|---|---|---|
| 1 | 0.93 | 0.97 | 0.89 | 0.98 | 0.96 | 0.99 | 0.97 |
| 2 | 0.93 | 0.74 | 0.72 | 0.73 | 0.68 | 0.94 | 1.00 |
| 4 | 0.92 | 0.51 | 0.62 | 0.45 | 0.48 | 0.84 | 0.82 |
| 8 | 0.57 | 0.25 | 0.39 | 0.21 | 0.24 | 0.35 | 0.37 |
| 12 | 0.39 | 0.18 | 0.31 | 0.15 | 0.17 | 0.21 | 0.14 |
| 16 | 0.33 | 0.12 | 0.27 | 0.11 | 0.12 | 0.18 | 0.07 |
