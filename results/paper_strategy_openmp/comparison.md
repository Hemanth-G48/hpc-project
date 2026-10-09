# Four benchmarks × four implementations (12 threads, pinned)

Sizes: map 63 vectors × 256 KB × 250 rounds · vecmul 128 blocks × 112 KB × 3 × 200 rounds ·
reduction 256 MB, depth 10 · matmul 1024², block 64.
Times in seconds. Parallel columns: best-of-5, pinned to physical P-cores first.
Sequential: best-of-3. All parallel results are bit-identical to sequential checksums.

| benchmark | sequential | OpenMP (loop) | ps runtime (std/coarse) | ps runtime (loc/coarse) | OMP 5 tasks (depend+affinity) |
|---|--:|--:|--:|--:|--:|
| map | 0.1887 | 0.0404 (4.7×) | 0.0361 (5.2×) | 0.0389 (4.9×) | **0.0292 (6.5×)** |
| vecmul | 0.3737 | 0.1764 (2.1×) | 0.1542 (2.4×) | 0.1685 (2.2×) | **0.1536 (2.4×)** |
| reduction | 0.3434 | 0.1712 (2.0×) | 0.1879 (1.8×) | 0.1811 (1.9×) | **0.1670 (2.1×)** |
| matmul | 0.2178 | 0.0474 (4.6×) | 0.0391 (5.6×) | 0.0407 (5.4×) | **0.0390 (5.6×)** |

Sources: `results/paper_strategy/strategy.csv` (loop OpenMP, ps runtime),
`results/paper_strategy_openmp/tasks.csv` (OpenMP 5 tasks), `./build/*_seq` (sequential).

Readings: with the same thread placement, the two "strategy" implementations (custom
runtime and OpenMP 5 tasks) are the fastest or tied on every benchmark; reduction is the
only kernel where the plain loop version beats one of them (the ps std/coarse config).
