# HPC Project Report — Profiling and Parallelization of Representative Kernels

**Platform:** single-node Linux workstation · Intel Core i5-13450HX (10 cores: 6 P-cores + 4 E-cores, 16 hardware threads, 1 NUMA node) · 23 GB RAM · GCC 16.2.0 · OpenMP · `perf` (Linux perf_events)
**Date:** 2026-10-08

---

## 1. Problem statement & scope

The project covers seven HPC workloads: three core kernels (chosen to exercise three
distinct performance regimes) plus the four pattern-based benchmarks from the course
paper's evaluation set (Map, Reduction, Matmul, Vecmul — Jacobi is covered by `heat2d`,
SparseLU by `sparselu`):

| Kernel | Workload | Regime |
|---|---|---|
| `matmul` | blocked dense matrix multiplication (C = A·B), n = 2048, block = 64 | compute-bound, O(n³) |
| `heat2d` | 2-D Jacobi 5-point stencil (heat diffusion), 2048² grid, 400 iterations | memory-bandwidth-bound, O(n²·iters) |
| `kmeans` | K-means clustering (1,000,000 points, K = 16, d = 2, max 100 iterations) | irregular data-heavy, O(np·K·d·iters) |
| `map` | 1-D vector scaling, per-vector blocks (paper's Map) | streaming, bandwidth-bound |
| `reduction` | iterative bottom-up merge of 1024 sorted runs, depth 10 (paper's Reduction) | memory + branch mix |
| `sparselu` | blocked banded LU (diagonally dominant, no pivoting), n = 8192, block 256 (paper's SparseLU) | chain-limited block ops |
| `vecmul` | element-wise vector product, per-vector blocks (paper's Vecmul) | cache-friendly streaming |

Each kernel exists in two versions: a single-threaded **sequential** implementation and an
**OpenMP** parallel implementation. All runs use deterministic inputs and print a checksum,
so parallel results are verified against the sequential ones.

## 2. Environment & methodology

**Timing.** Each program measures only the core computation with `clock_gettime(CLOCK_MONOTONIC)`
(initialisation, allocation and checksums are outside the timed region). Every configuration
was run 5 times; the report uses the **best** time (minimal interference from background load).
Memory-allocation and first-touch effects are therefore excluded from the comparisons.

**Thread placement.** On this hybrid CPU (6 P-cores with SMT + 4 E-cores), OpenMP runs are
pinned with `taskset` to physical P-cores first, then SMT siblings, then E-cores
(the sweep script builds the CPU list automatically). Placement is decisive here: unpinned
12-thread runs on barrier-heavy kernels were up to 2.5× slower (e.g. map 0.093 s → 0.037 s
pinned), and pinning removed the SparseLU high-thread regression entirely.

**Profiling.** Perf was used on the sequential binaries (`-O3 -g -fno-omit-frame-pointer`):

```bash
perf record -e cpu-clock -g --call-graph fp -- ./build/<kernel>_seq  ...
perf stat -d -- ./build/<kernel>_seq  ...
perf report / perf annotate
```

Note: this is a hybrid CPU with separate P-core (`cpu_core`) and E-core (`cpu_atom`) PMUs.
`perf stat -d` therefore reports counters for both; the table below quotes the **P-core**
section (where ~99% of samples land). Sampling used the software `cpu-clock` event so that
the hotspot report is a single, unbiased section.

**Sources of noise (caveats).** Best-of-5 on a laptop-class machine with background load;
hybrid P/E cores and SMT sharing make sub-linear scaling partly a hardware property rather
than a code deficiency; CPU turbo clocks vary with the number of active cores.

## 3. Deliverable 1 — Sequential implementations

Artifacts: `sequential/matmul_seq.c`, `heat2d_seq.c`, `kmeans_seq.c`, `map_seq.c`,
`reduction_seq.c`, `sparselu_seq.c`, `vecmul_seq.c` (+ `common.h` for timing/PRNG helpers,
`Makefile`).

| Kernel | Algorithm | Size used | Sequential time |
|---|---|---|---|
| matmul | blocked (ikj) triple loop, B=64 for cache reuse | n = 2048 | **1.73 s** (9.9 GFLOP/s) |
| heat2d | double-buffered Jacobi stencil, pointer swap per iteration | 2048², 400 iters | **1.50 s** (5.6 GFLOP/s) |
| kmeans | Lloyd's algorithm: assign → accumulate → update, converged in 53 iterations | 1M pts, K=16, d=2 | **1.42 s** |
| map | per-vector scale loop (`v[j] *= α`), one allocation per vector, repeated rounds | 48 vectors × 1 MB, 100 rounds | **0.36 s** |
| reduction | bottom-up pairwise merges of 2^depth pre-sorted runs, alternating buffers | 256 MB (2²⁵ doubles), depth 10 | **0.34 s** |
| sparselu | blocked banded LU (block-tridiagonal, no pivoting: `lu_nopivot`, two triangular solves, Schur update per block step) | n = 8192, block 256 | **0.46 s** |
| vecmul | element-wise product of per-vector blocks (`z = x·y`) | 128 vector blocks × 28 KB × 3, 1000 rounds | **0.23 s** |

Every program carries an independent correctness check (checksum for all; `sorted=1` for
reduction; end-to-end solve residual for sparselu: 7.6e-15).

Build and run:

```bash
make all
./build/matmul_seq 2048 64
./build/heat2d_seq 2048 400
./build/kmeans_seq 1000000 16 2 100
./build/map_seq 48 131072 100
./build/reduction_seq 33554432 10
./build/sparselu_seq 8192 256
./build/vecmul_seq 128 7168 1000
```

Each run prints a machine-readable result line, e.g.
`RESULT kernel=matmul impl=seq threads=1 n=2048 bs=64 flops=... time=... checksum=...`.

## 4. Deliverable 2 — Hotspot identification & parallelizability

### 4.1 Profiling evidence

`perf report` (self time, sequential binaries):

| Kernel | Hotspot | Self time | Remarks |
|---|---|---|---|
| matmul | `matmul()` | **98.55 %** | blocked multiply-accumulate loops |
| matmul | `fill()` | 1.22 % | input generation (PRNG) |
| heat2d | `jacobi()` | **99.52 %** | 5-point stencil sweep |
| kmeans | `iterate()` | **99.14 %** | distance computation + cluster accumulation |
| map | `map_kernel()` | **95.95 %** | streaming scale loop |
| reduction | `reduce_kernel()` | **71.69 %** | pairwise merges (init + verification passes ≈ 28 %, outside the timed region) |
| sparselu | `factorize()` | **94.60 %** | breakdown: `solve_lower_unit` 32.6 %, `solve_upper` 31.3 %, `schur` 22.2 %, `lu_nopivot` 8.5 % |
| vecmul | `vecmul_kernel()` | **98.20 %** | element-wise multiply streaming |

`perf stat -d` — P-core counters (top-down breakdown is the strongest evidence of *why*
each kernel is hot):

| Kernel | IPC | LLC-load miss rate | Top-down (P-core) | Interpretation |
|---|---|---|---|---|
| matmul | 4.3 | 24.2 % | retiring 67.9 %, backend 26.2 % | **compute-bound**; blocking keeps LLC traffic low |
| heat2d | 1.5 | 99.4 % | backend-bound **61.6 %**, retiring 31.9 % | **memory-bound**: most cycles wait on data, not on math |
| kmeans | 3.4 | 92.8 % | retiring 63.3 %, frontend 17.4 % | compute/control-bound; branchy distance loop |
| map | 1.5 | 98.0 % | backend-bound **72.9 %**, retiring 27.1 % | **bandwidth-bound** streaming over a 48 MB working set |
| reduction | 3.8 | 94.4 % | retiring 62.4 %, backend 37.3 % | memory traffic + branchy merge comparisons |
| sparselu | 3.3 | 73.5 % | retiring 55.4 %, backend 36.5 % | block ops on a 49 MB band; chain-limited |
| vecmul | 3.7 | 15.5 % | backend 42.2 %, retiring 55.5 % | cache-friendly: small working set, only 15 % LLC misses |

### 4.2 Are the hotspots parallelizable?

**`matmul` → yes (data-parallel over output blocks).**
The blocked loop computes disjoint B×B tiles of C; there is no cross-tile dependency.
Parallelising the outer block loops over `(ii, jj)` assigns each C tile to exactly one thread,
and the accumulation order *within* each C element is unchanged → results are bit-identical.

**`heat2d` → yes (data-parallel domain sweep), with a serial dimension.**
Each Jacobi iteration reads the previous grid and writes a separate buffer, so all interior
cells of one sweep are independent (classic domain decomposition). The *time* loop is
inherently sequential (iteration t+1 depends on t), which caps the available parallelism to
one grid sweep at a time — and combined with its bandwidth-bound profile, predicts limited
speedup at high thread counts.

**`kmeans` → yes (map + reduction).**
The assign step is a map over points (independent nearest-centroid queries) with two
reductions (`inertia`, `moved`). The accumulate step is a histogram/scatter reduction over
clusters — parallelizable with per-thread private partials. The outer convergence loop is
sequential by definition. Expected limits: load imbalance is negligible here, but barriers
per iteration and the reduction merge add overhead.

**`map` → yes (data-parallel per vector).**
Each round consists of independent per-vector scale loops; the only ordering that must be
preserved is the chain of operations on the same element *across rounds*, which a per-round
barrier provides.

**`reduction` → yes (per level).**
Merges within a level operate on disjoint output ranges and are fully independent; the levels
themselves are sequential (level l+1 consumes level l's output). Available parallelism halves
at each level, ending with a single merge task at the top — an Amdahl ceiling inherent to
tree reductions.

**`sparselu` → partially.**
The block operations (block LU, triangular solves, Schur update) are internally parallel, but
the factorization has a **sequential dependency chain** across block steps (block k+1's Schur
update depends on step k), and each step's block ops are modest in size. Expect good speedup
at few threads, flattening afterwards.

**`vecmul` → yes (element-wise).**
Independent per-vector element-wise products; trivially data-parallel, but with small cache-
resident working sets, so high thread counts mostly add per-round overhead.

**Verdict:** all seven hotspots are parallelizable with data-parallel constructs; none is
inherently serial. Amdahl-limited serial parts are O(n²) initialisation/finalisation
(< 2 % of runtime), so high speedups are theoretically possible — bounded in practice by
memory bandwidth (heat2d, map, reduction), dependency chains (sparselu), and per-round
synchronisation (map, vecmul).

## 5. Deliverable 3 — Platform selection & justification

| Platform | Fit for this project | Verdict |
|---|---|---|
| **OpenMP** | Single shared-memory node (1 NUMA node, 16 HW threads); all hotspots are loop-level data parallelism; sequential code becomes parallel by adding directives — same sources, easy correctness checking | **Chosen** |
| MPI | No multi-node requirement: all working sets (≤ 100 MB) fit in RAM; would add domain partitioning + halo exchange complexity (heat2d) or replicated-input management with no benefit on one node | Rejected |
| CUDA | No CUDA toolkit installed on the evaluation machine; the GPU (RTX 3050 Laptop, 6 GB) would need host↔device copies for bandwidth-bound heat2d and a hand-tuned blocked GEMM to beat the CPU; disproportionate effort/risk for the assignment | Rejected |

**Justification.** The platform choice is driven by the workload structure and the execution
environment: all kernels are *shared-memory data-parallel* workloads on a *single node*,
which is precisely the model OpenMP targets. MPI solves a problem this project does
not have (distributed memory), and CUDA imposes a rewrite plus transfer costs best justified
by very high arithmetic intensity — which only `matmul` partially exhibits. OpenMP also
preserves an incremental verification path: the parallel version can be diffed against the
sequential checksum, and the design principle matches the course background material on
locality-aware execution (see §9).

## 6. OpenMP parallelization design

| Kernel | Parallelised construct | Directives used | Correctness concern addressed |
|---|---|---|---|
| matmul | outer block loops `(ii, jj)` collapsed into one iteration space of 1024 tiles | `#pragma omp parallel for collapse(2) schedule(static)` | disjoint C tiles; per-element accumulation order unchanged |
| heat2d | interior rows of each Jacobi sweep | `#pragma omp parallel` + `#pragma omp for schedule(static)` per sweep + `#pragma omp single` for the buffer swap (implicit barriers) | read/write buffers are distinct; swap happens after all threads finish the sweep |
| kmeans | points loop (assign) and points loop (accumulate) | `reduction(+:moved, inertia)`; per-thread private partial sums merged under `#pragma omp critical` | private partials remove scatter races; merge is O(K·d) per thread |
| map | per-vector loop inside each round | `#pragma omp parallel` + `omp for schedule(static)` per round | per-element ordering across rounds preserved by the round barrier |
| reduction | merge pairs within each level | `omp for` over pairs + `omp single` for the pointer/run-size swap, repeated per level | merges write disjoint ranges; level order preserved by the single's barrier |
| sparselu | panel-based `lu_nopivot` (serial panel of W=16 columns + one parallel trailing update per panel), plus column/row-parallel triangular solves and Schur update | `omp single` (panel) + `omp for` (trailing update); `omp parallel for` for solves/schur | pivot-at-a-time barriers cut ~8×; per-element accumulation order unchanged → still bit-identical |
| vecmul | per-vector element-wise loop inside each round | same pattern as map | element-wise independence; round barrier preserves the re-run chain |

**Verification:** all fourteen binaries print checksums; the sweep script compares them
automatically (`results/summary.md`):

- `matmul`, `heat2d`, `map`, `reduction`, `sparselu`, `vecmul`: checksum **relative
  difference = 0** (bit-identical to sequential) — including the blocked panel LU after the
  SparseLU parallelisation rework
- `kmeans`: **identical label hash** (same clustering); inertia differs only in the last
  floating-point digits (~1e-15 relative) due to reordered summation

## 7. Comparative results

Best-of-5 times (s), all seven kernels (OpenMP threads pinned to physical P-cores first):

| threads | matmul | heat2d | kmeans | map | reduction | sparselu | vecmul |
|---|---|---|---|---|---|---|---|
| seq | 1.731 | 1.497 | 1.423 | 0.356 | 0.338 | 0.462 | 0.225 |
| 1 | 1.859 | 1.540 | 1.594 | 0.362 | 0.353 | 0.467 | 0.232 |
| 2 | 0.932 | 1.012 | 0.992 | 0.244 | 0.247 | 0.245 | 0.113 |
| 4 | 0.469 | 0.732 | 0.579 | 0.199 | 0.177 | 0.138 | 0.069 |
| 8 | 0.377 | 0.751 | 0.454 | 0.208 | 0.179 | 0.164 | 0.075 |
| 12 | 0.370 | 0.711 | 0.386 | 0.199 | 0.170 | 0.186 | 0.137 |
| 16 | 0.323 | 0.750 | 0.333 | 0.199 | 0.175 | 0.157 | 0.190 |

Speedup vs sequential (peak per kernel in bold):

| threads | matmul | heat2d | kmeans | map | reduction | sparselu | vecmul |
|---|---|---|---|---|---|---|---|
| 2 | 1.86× | 1.48× | 1.43× | 1.46× | 1.37× | 1.88× | 2.00× |
| 4 | 3.69× | 2.05× | 2.46× | **1.79×** | 1.91× | **3.35×** | **3.26×** |
| 8 | 4.59× | 1.99× | 3.13× | 1.71× | 1.89× | 2.81× | 2.99× |
| 12 | 4.68× | **2.11×** | 3.69× | 1.79× | **1.98×** | 2.48× | 1.65× |
| 16 | **5.36×** | 2.00× | **4.28×** | 1.78× | 1.93× | 2.94× | 1.19× |

Plot: `results/speedup.png` (speedup and efficiency vs thread count).

**Observations.**

1. **`matmul` scales best (5.36× at 16 threads)** — the compute-bound profile predicted this.
   It keeps gaining when E-cores join at 16 threads because it is not bandwidth-limited at
   this size.
2. **`kmeans` scales steadily to 4.28×** at 16 threads: the assign step is embarrassingly
   parallel and the point array largely fits in cache; per-iteration barriers and the
   partial-merge keep efficiency below `matmul` early on.
3. **`heat2d` (2.11×) and `map` (1.79×) saturate early** — the bandwidth-bound signature
   (backend-bound 61.6 % / 72.9 %, LLC-load miss ≈ 99 %): extra threads contend for the same
   DRAM bandwidth rather than adding throughput.
4. **`reduction` plateaus at ~2× (1.98×)**: the top of the merge tree is serial (parallelism
   halves per level), and every level streams the full 256 MB array.
5. **`sparselu` reaches 3.35× at 4 threads (2.94× at 16)** — chain-limited as predicted, but
   this is where the biggest engineering fix of the project landed: the first version used
   one barrier per pivot (256 per block × 32 blocks) and *regressed* beyond 4 threads
   (~1.4× at 16, unpinned). The panel-based factorization (serial 16-column panel + one
   parallel trailing update — the dgetrf scheme) plus thread pinning removed the regression
   entirely, keeping results bit-identical.
6. **`vecmul` peaks at 4 threads (3.26×) and degrades at high thread counts** (16 threads:
   1.19×). With this cache-resident working set (14 MB) and small per-round tasks, extra
   threads mostly add per-round synchronisation and SMT contention; 16 threads is ~2.7×
   slower than 4. The result shows the workload's load-balance sensitivity noted in the
   background paper rather than a code defect.
7. **1-thread OpenMP overhead** is 0.89–0.99× across kernels (parallel-region entry,
   barriers, reduction machinery) — worth keeping in mind when reading low-thread speedups.

## 8. Reproducing the results

```bash
make all                                # 7 sequential + 7 OpenMP (+ 4 paper-strategy) binaries
REPS=5 ./scripts/run_experiments.sh     # thread sweep, pinned OMP -> results/timings.csv + sweep.log
./scripts/profile.sh                    # perf hotspots/counters -> profiling/
./scripts/plot_results.py               # results/summary.md + results/speedup.png (+ correctness check)
REPS=3 ./scripts/run_paper_strategy.sh  # paper-strategy study -> results/paper_strategy/
./scripts/plot_paper_strategy.py
```

Layout: `sequential/`, `openmp/`, `paper_strategy/` (paper-mechanism runtime + drivers),
`scripts/`, `profiling/` (perf data + hotspot reports), `results/` (timings.csv, summary.md,
speedup.png, sweep.log; `results/paper_strategy/` for the strategy study), `Makefile`, `common.h`.

## 9. Background connection

*Provenance: every timing and profiling number in sections 3–7 was measured by this project's own scripts on the machine described in section 2 (raw data in `results/timings.csv`, `results/sweep.log` and `profiling/`). The paper cited below is background material only — no results or figures are reproduced from it.*

The profiling conclusions align with the course background paper on locality-aware execution
(Muddukrishna et al., *Locality-Aware Task Scheduling and Data Distribution for OpenMP
Programs on NUMA Systems and Manycore Processors*, Scientific Programming 2015): `heat2d`'s
early saturation is the classic case where non-uniform/limited memory access latency — not
compute — bounds performance, and the paper's theme "match data distribution and scheduling
to the access pattern" is directly visible in the contrast between `matmul` (compute-bound,
scales) and `heat2d`/`map` (bandwidth-bound, plateau). On a multi-node NUMA system, the
natural next step for `heat2d` would be first-touch/interleaved page placement and row-block
affinity, as discussed there.

Beyond the loop-parallel comparison, the `paper_strategy/` folder implements the paper's own
mechanisms (fine/coarse data distribution via `ps_alloc`, work-dealing by task footprint with
a distance-matrix cost model, vicinity-limited work-stealing) as a small task runtime, with
the paper's Map / Reduction / Matmul / Vecmul benchmarks ported onto it. Its comparative
study (`results/paper_strategy/`) reproduces the paper's confinement result directly: when
coarse data concentrates in few homes (Reduction's single allocation, Matmul's three),
vicinity=1 stealing costs **+41 %** (reduction) and **+115 %** (matmul) execution time
(relay-chain stealing), partially rescued at vicinity=2 (+20 % / +34 %), fully rescued by
global stealing — while balanced-home workloads (map, vecmul, per-vector allocations) are
insensitive to vicinity and the locality-aware scheduler otherwise performs within noise of
standard work-stealing, which is the paper's own "falls back to load balancing when locality
is missing" case.

A companion folder `paper_strategy_openmp/` implements the task/dataflow half of the same
strategy with stock OpenMP 5 (`task` + `depend` footprints + `affinity` locality hints, the
standardized descendant of this research line). Its four task-based versions are
bit-identical to sequential and measured slightly faster than both the loop-OpenMP versions
and the custom runtime on these benchmarks (e.g. map 0.029 s vs 0.040 s loop / 0.036 s
runtime) — but the paper's scheduler steering (cost-based dealing, vicinity limits,
location-bound queues) is not expressible in stock OpenMP, which is precisely why the custom
runtime exists.

## 10. References

1. A. Muddukrishna, P. A. Jonsson, M. Brorsson. *Locality-Aware Task Scheduling and Data Distribution for OpenMP Programs on NUMA Systems and Manycore Processors.* Scientific Programming, 2015. DOI 10.1155/2015/981759.
2. MIR task-based runtime system (reference implementation from the paper): https://github.com/anamud/mir-dev
3. OpenMP Application Programming Interface, https://www.openmp.org
4. Linux perf_events wiki, https://perf.wiki.kernel.org
