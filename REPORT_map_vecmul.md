# Submission — map & vecmul: sequential implementation · hotspots · platform choice

Covers the three required items for the two pattern-based benchmarks (Map and Vecmul).
Machine: Intel i5-13450HX (6 P-cores + 4 E-cores / 16 threads, single NUMA node), GCC 16.2.
All numbers in this document were measured by this project's harness (raw data: `results/`,
`profiling/`).

---

## Item 1 — The sequential implementation

| Program | What it computes | Default workload |
|---|---|---|
| `sequential/map_seq.c` | **Map** — 1-D vector scaling: for each vector block, `v[j] *= α` for all elements, repeated over rounds | 48 vectors × 1 MB, 100 rounds |
| `sequential/vecmul_seq.c` | **Vecmul** — element-wise vector product: `z[j] = x[j] · y[j]` per vector block, repeated over rounds | 128 blocks × 28 KB × 3 arrays, 1000 rounds |

Implementation notes:

- C11, single-threaded, one loop nest per kernel, optional `(nv, len, reps)` arguments.
- Deterministic inputs (xorshift64 PRNG, fixed seeds) so runs are reproducible.
- Timing: `clock_gettime(CLOCK_MONOTONIC)` around the core computation only
  (allocation, initialization and checksum are outside the timed region); best-of-5.
- Each run prints a machine-readable result line with time **and a checksum** that later
  parallel versions must reproduce.

Build and run:

```bash
make seq
./build/map_seq 48 131072 100        # nv=48 vectors, len=131072 doubles (1 MB), 100 rounds
./build/vecmul_seq 128 7168 1000     # nv=128 blocks, len=7168 ints (28 KB), 1000 rounds
```

Measured sequential performance: **map 0.356 s**, **vecmul 0.225 s**.

## Item 2 — Identified hotspots, and why they are (or are not) parallelizable

Method: `perf record -e cpu-clock -g --call-graph fp` + `perf stat -d` on `-O3 -g
-fno-omit-frame-pointer` binaries; reports in `profiling/`.

| Kernel | Hotspot | Self time | Hardware counters (P-core) | Interpretation |
|---|---|---|---|---|
| map | `map_kernel()` | **95.95 %** | IPC 1.5 · LLC-load miss **98.0 %** · **backend-bound 72.9 %** (retiring 27.1 %) | streaming a 48 MB working set that exceeds L3 → **DRAM-bandwidth-bound** |
| vecmul | `vecmul_kernel()` | **98.20 %** | IPC 3.7 · LLC-load miss 15.5 % · backend 42.2 %, retiring 55.5 % | ~11 MB working set stays cache-resident; modest memory pressure |

(Remainder: initialization/checksum in `main`, outside the timed region — map 3.98 %,
vecmul 1.70 %.)

**map — parallelizable: yes.** The hotspot is a per-vector scaling loop. Within a round,
every vector block is an independent unit of work — no cross-vector dependencies, no
reductions, nothing shared. The only ordering requirement is the per-element operation
chain *across successive rounds* (round r+1 scales values produced by round r), which a
single barrier per round preserves. It is therefore embarrassingly data-parallel.
*Caveat from the profile:* the counter evidence (98 % LLC misses, 72.9 % backend-bound)
says the kernel is limited by DRAM bandwidth — so the expected speedup saturates with the
memory system rather than scaling linearly with threads.

**vecmul — parallelizable: yes.** `z[j] = x[j]·y[j]` has no loop-carried dependencies at
all; blocks are independent and rounds are re-executions of independent work (a round
barrier suffices for ordering). Fully data-parallel. Being cache-friendly, it should scale
initially; the limits are the small per-task work and per-round synchronization at very
high thread counts.

**Verdict:** both hotspots are embarrassingly parallel; no algorithmic restructuring is
required. Predicted limits: map → memory bandwidth; vecmul → synchronization/contention at
high thread counts.

## Item 3 — Chosen platform for parallelization, with justification → **OpenMP**

| Platform | Assessment |
|---|---|
| **OpenMP (chosen)** | Single shared-memory node (1 NUMA node, 16 hardware threads); both hotspots are loop-level data parallelism over independent elements — the canonical OpenMP case. The sequential code becomes parallel by adding directives only, and results stay verifiable against the sequential checksums (bit-identical in practice). Thread affinity (`taskset` here; `OMP_PLACES` + `OMP_PROC_BIND` natively) handles the hybrid P/E-core topology. |
| MPI (rejected) | No distributed-memory requirement: working sets (≤ 50 MB) fit in RAM. Partitioning + communication would add complexity and overhead with zero benefit on a single node. |
| CUDA (rejected) | No CUDA toolkit on the evaluation machine; both kernels have low arithmetic intensity and are memory-bandwidth-bound, so GPU compute is wasted while host↔device copies dominate. Disproportionate rewrite for no expected gain. |

**Justification stated plainly:** the hotspots are shared-memory, loop-level, data-parallel
workloads on a single node → the platform must be a shared-memory threading model → OpenMP
is the direct, lowest-risk match, with an incremental path from the sequential code and
checksum-based verification of every parallel result.

**The choice is validated by measurement:** under OpenMP, results remained bit-identical to
sequential, and the measured scaling matched the profile's predictions exactly — map
saturated at **~1.8×** (the bandwidth wall), while vecmul peaked at **3.26× on 4 threads**
and declined at very high thread counts (synchronization). Full numbers in
`results/summary.md` and `results/speedup.png`.
