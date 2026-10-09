# HPC Project — Profiling and Parallelization of Representative Kernels

Course project: sequential implementations of representative HPC kernels, profiling-driven
hotspot analysis, OpenMP parallelization, and two implementations of the background paper's
locality-aware strategy (a custom task runtime and a stock OpenMP 5 tasks variant).

## Layout

| Path | Contents |
|---|---|
| `sequential/` | 7 single-threaded kernels: matmul, heat2d, kmeans, map, reduction, sparselu, vecmul |
| `openmp/` | loop-parallel OpenMP versions (bit-identical results) |
| `paper_strategy/` | mini task runtime implementing the paper's mechanisms (work-dealing, fine/coarse distribution, vicinity-limited work-stealing) + benchmark drivers |
| `paper_strategy_openmp/` | OpenMP 5 task versions (`depend` footprints + `affinity` hints) |
| `scripts/` | sweep, profiling, plotting and strategy-study harnesses |
| `profiling/` | perf hotspot and hardware-counter reports |
| `results/` | timings, summaries, plots, strategy studies |
| `REPORT.md` | full project report (hotspots, platform justification, comparative results) |
| `REPORT_map_vecmul.md` | focused stage report for map & vecmul |

## Quick start

```bash
make all                                  # build everything (make seq / omp / ps / tasks also exist)
REPS=5 ./scripts/run_experiments.sh       # thread sweep -> results/timings.csv + summary
./scripts/profile.sh                      # perf reports -> profiling/
./scripts/plot_results.py                 # results/summary.md + results/speedup.png
REPS=5 ./scripts/run_paper_strategy.sh    # paper-strategy runtime study
REPS=5 ./scripts/run_paper_strategy_openmp.sh
```

## Headline results (best-of-5, 12 threads, threads pinned to physical P-cores first)

- OpenMP speedups vs sequential: matmul 5.4x, kmeans 4.3x, sparselu 3.4x, vecmul 3.3x,
  heat2d 2.1x, reduction 2.0x, map 1.8x (bandwidth-limited).
- Paper-strategy study: locality-aware scheduling is within noise of standard work-stealing
  on this uniform-memory machine (safe default), while vicinity=1 stewardship serializes
  concentrated workloads (reduction +41 %, matmul +115 % — the paper's confinement effect).
- OpenMP 5 tasks (`depend` + `affinity`) match or beat both the loop-OpenMP versions and the
  custom runtime on the four paper benchmarks — every parallel result is bit-identical to
  its sequential counterpart.

See `REPORT.md` for details and `results/` for raw data.
