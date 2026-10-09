# Detailed discussion — `map` & `vecmul`: sequential implementation, hotspots, platform choice

Deep-dive companion to `REPORT_map_vecmul.md` (the three-item submission). This document
explains *why* each claim holds — the reasoning, the arithmetic, and the counter evidence —
so every number can be defended. All measurements come from this project's own harness
(`scripts/`, raw data in `results/` and `profiling/`).

Reference machine: Intel i5-13450HX — 6 P-cores + 4 E-cores (16 hardware threads),
L1d 48 KB/core, L2 1.3 MB (P-core, private; E-cores share a ~2 MB cluster L2),
**L3 20 MiB (shared)**, 1 NUMA node, GCC 16.2.

---

## 1. Item 1 — The sequential implementation

### 1.1 What each program computes, and why the shape is what it is

**`map_seq.c` — Map (1-D vector scaling).** `nv` vectors of `len` doubles live in one flat
allocation (48 vectors × 1 MB = 48 MB). One round = for every vector block, multiply every
element by α (`v[j] *= α`); the benchmark runs 100 rounds.

- *Why per-vector blocks:* mirrors the paper's Map benchmark ("each task scales a separate
  vector in a list") and gives natural block granularity — the unit the data-distribution
  and scheduling questions operate on.
- *Why α ≈ 1 (0.9999999):* keeps values bounded across 100 rounds (no overflow, no denormal
  drift), so the kernel is pure memory traffic — which is exactly what makes its
  bandwidth-boundedness visible.
- *Why rounds:* repeated sweeps create (a) a natural barrier structure for parallel
  versions, and (b) sustained traffic so cache/DRAM behavior — not cold-start effects — is
  what gets measured.

**`vecmul_seq.c` — Vecmul (element-wise vector product).** `nv` blocks, each holding three
sections (x, y, z) of `len` ints; each round computes `z[j] = x[j]·y[j]` for every block.
128 blocks × 3 × 28 KB → an ~11 MB working set.

- *Why integers:* the paper's TILEPro64 runs used integer kernels; inputs < 1000 keep
  products < 10⁶ (no overflow in `int`).
- *Why this shape:* zero-dependency element-wise arithmetic — the cleanest possible
  data-parallel kernel, useful as the "should scale well" counterpart to Map's
  "will saturate" case.

### 1.2 Measurement discipline (why the baseline numbers are trustworthy)

- **Timing region:** `clock_gettime(CLOCK_MONOTONIC)` wraps *only* the core loop —
  allocation, PRNG initialization and the checksum are outside it. Otherwise the
  measurement would reflect the PRNG/verification, not the kernel.
- **Best-of-5:** laptop-class noise; the minimum approximates an interference-free run.
- **Determinism:** xorshift64 with fixed seeds → identical inputs in every run and every
  implementation, so checksums are directly comparable (see below).
- **`NOINLINE` on the kernel functions:** so `perf` attributes samples to a named kernel
  symbol (`map_kernel`, `vecmul_kernel`) instead of everything merging into `main`.
- **Parameterized sizes** (`nv len reps` on the command line): one binary serves smoke
  tests, profiling at report sizes, and full sweeps.
- **Checksum after every run:** a serial-order sum of the result array. The parallel
  versions must reproduce it — this is the correctness gate that later let us verify
  bit-identical results (stronger than tolerance-based comparison).

### 1.3 Reading the baseline numbers (arithmetic, not just seconds)

| kernel | work | traffic | time | implied rate |
|---|---|---|---|---|
| map | 48 × 131072 × 100 = **629 M element updates** | 48 MB read + 48 MB write per round → **9.6 GB total** | 0.356 s | **≈ 27 GB/s single-threaded** |
| vecmul | 128 × 7168 = 0.92 M element products per round × 1000 = **917 M products** | 11 MB per round × 1000 → **11 GB total** | 0.225 s | **≈ 49 GB/s** |

The two rates already tell the story:
- Map's 27 GB/s is near what a single modern core can pull from DRAM — the sequential
  implementation is *already saturating its memory path*. Any parallel version can only
  add as much bandwidth as the platform has left (measured aggregate ceiling ≈ 48–50 GB/s),
  i.e. at most ~1.8×  — exactly the speedup measured later.
- Vecmul's 49 GB/s is *higher than DRAM streaming*, which is only possible because its
  11 MB working set fits in the 20 MiB L3 — reads/writes mostly hit L3, not DRAM. This is
  confirmed by the counters in §2 (only 15.5 % LLC-load misses).

---

## 2. Item 2 — Identified hotspots and why they are parallelizable

### 2.1 How the hotspots were found (method)

- `perf record -e cpu-clock -g --call-graph fp -- ./build/<kernel>_seq …` — a **software
  event**; on this hybrid CPU (P-cores and E-cores have separate PMUs) hardware events
  split the report into sections, while `cpu-clock` yields one unbiased profile over all
  cores. `-g` records call graphs so the time is attributed to functions.
- `perf stat -d` — counters: IPC, L1d/LLC behavior, and the **top-down breakdown**
  (retiring / frontend-bound / backend-bound / bad-speculation), which answers *why* the
  pipeline is slow, not just *where*.
- Reports saved per kernel in `profiling/` (`*.hotspots.txt`, `*.perfstat.txt`,
  `*.annotate.txt`).

How to read the headline counters:
- **IPC** (instructions per cycle): modern cores retire up to ~4–5; lower means stalls.
- **retiring** = % of pipeline slots doing useful work.
- **backend-bound** = % of slots stalled waiting on the memory subsystem.
- **LLC-load miss rate** = fraction of last-level-cache loads that came from DRAM.

### 2.2 `map` — hotspot analysis

**Hotspot:** `map_kernel()` — **95.95 %** of samples. (`main` 3.98 % = PRNG fill + checksum,
which are outside the timed region.)

**Counters (P-core):** IPC **1.5** · LLC-load miss **98.0 %** · **backend-bound 72.9 %** ·
retiring 27.1 %.

**Diagnosis chain:** 98 % LLC misses ⇒ virtually every cache line comes from DRAM — the
48 MB working set cannot fit the 20 MiB L3, so each of the 100 rounds re-streams from main
memory. IPC 1.5 + 72.9 % backend-bound + only 27.1 % retiring ⇒ the pipeline spends ~3 of
every 4 slots waiting on memory, not computing. The kernel is a *memory pipe*; the
arithmetic (one multiply per element) is irrelevant to its speed.

**Parallelizability (dependency analysis, not vibes):**
- Within a round, every operation `v[k][j] *= α` touches a distinct `(k, j)` element — no
  reads of neighbors, no shared state, no reductions, no atomics.
- The only true dependency is **per element across rounds**: round r+1 consumes the value
  round r produced. That chain is preserved by a single barrier between rounds.
- Conclusion: embarrassingly data-parallel; no restructuring needed.

**Predicted ceiling — stated before running the parallel version:** DRAM bandwidth. One
thread already moves ~27 GB/s; the platform's measured aggregate ceiling is ~48–50 GB/s,
so the *best possible* speedup is ≈ 48/27 ≈ **1.8×**. Measured: **1.79× plateau** from 4
threads upward. The prediction (from counters) and the measurement agree.

### 2.3 `vecmul` — hotspot analysis

**Hotspot:** `vecmul_kernel()` — **98.20 %** of samples.

**Counters (P-core):** IPC **3.7** · LLC-load miss **15.5 %** · backend 42.2 % ·
retiring 55.5 %.

**Diagnosis:** the 11 MB working set fits in L3 — only 15.5 % of LLC loads go to DRAM;
most traffic is served on-chip. IPC 3.7 and 55.5 % retiring say the cores keep up with the
arithmetic comfortably. This kernel is *not* bandwidth-starved at one thread (as §1.3
already suggested by its 49 GB/s effective rate).

**Parallelizability:** `z[j] = x[j]·y[j]` — each output element depends on exactly two
inputs at the same index; no cross-element reads, no loop-carried dependencies, no
reductions. Rounds re-execute the same independent work (rewriting z with identical
values), so a per-round barrier is all the ordering that's needed.

**Predicted ceiling — stated before running:** not bandwidth (L3 serves), but
*per-round fixed costs*: the benchmark's task size is small (each block ~2–3 µs of work per
round) and every round ends at a barrier; as threads increase, synchronization overhead and
core contention (SMT/turbo on a hybrid CPU) grow faster than the useful work shrinks per
thread. Measured: peak **3.26× at 4 threads**, declining to 1.99×/1.65×/1.19× at
8/12/16 threads. Again, counter-based prediction == measurement.

### 2.4 Verdict table

| kernel | hotspot | dependency structure | parallelizable? | predicted ceiling | measured |
|---|---|---|---|---|---|
| map | 95.95 % | none within a round; per-element chain across rounds | **yes** | DRAM bandwidth ≈ 1.8× | 1.79× plateau |
| vecmul | 98.20 % | none at all (element-wise) | **yes** | per-round sync at high threads | 3.26× peak @ 4 threads |

---

## 3. Item 3 — Platform choice, explained

### 3.1 The decision framework

Four questions, in order:
1. What **structure of parallelism** do the hotspots have? (loop-level independent
   elements → shared-memory model; distributed data → message passing; dense compute →
   accelerator.)
2. Do the **data sizes** fit the candidate platform's memory? (This project: ≤ 50 MB —
   comfortably one node.)
3. What does the **execution environment** actually support? (What is installed; what will
   the evaluation machine run?)
4. What is the **risk/effort** of each option relative to its expected gain?

### 3.2 OpenMP (chosen) — the case, line by line

- **Match to structure:** both hotspots are loop-level data parallelism over independent
  array elements in a single address space — the canonical OpenMP situation; worksharing
  constructs map 1:1 onto the parallelism found in §2.
- **Incremental risk:** the sequential code becomes parallel by adding directives only; the
  same source compiles both versions; and correctness is checkable against the sequential
  checksum — which we achieved **bit-identically** for both kernels.
- **Machine specifics:** 16 hardware threads on one NUMA node. The hybrid P/E topology is
  not a detail: unpinned threads on this CPU measured up to **2.5× slower** on
  barrier-heavy kernels; OpenMP offers the standard remedy (`OMP_PLACES` +
  `OMP_PROC_BIND`; `taskset` here), which we used for every reported run.
- **Precedent:** the course background paper's runtime implements the OpenMP task model;
  OpenMP is the ecosystem norm for exactly this class of work.

### 3.3 MPI — rejected, explained

- **No distributed-memory requirement:** the working sets (48 MB, 11 MB) fit in one node's
  RAM trivially; there is no larger problem waiting behind the benchmark.
- **Cost without benefit:** MPI would demand domain partitioning, index/ghost bookkeeping,
  and communication for the checksum — while on a single machine every "rank" shares the
  same memory system anyway, so the communication is pure overhead. On top of that, MPI is
  not installed on the evaluation machine; installing and validating it for an expected
  gain of zero adds only failure modes.

### 3.4 CUDA — rejected, explained

- **Practical:** no CUDA toolkit on the evaluation machine — a CUDA version would not even
  be runnable there.
- **Technical (the deeper reason):** both kernels have arithmetic intensity of ~1
  operation per byte — pure streaming. Accelerators pay off when *compute* is the
  bottleneck; here the bottleneck (map: DRAM bandwidth; vecmul: per-round sync) is not
  something the GPU relieves. The laptop GPU's memory bandwidth is in the same class as the
  CPU's, and a CUDA port would add host↔device transfers for x, y, z and the verification
  pass — new traffic that the CPU version never pays. Expected gain ≈ zero; rewrite cost
  high; correctness risk real.

### 3.5 Validation, and where the choice would change

- **Validation by prediction:** the counter-based predictions in §2 (map: bandwidth ≈ 1.8×;
  vecmul: peak then decline) matched the measured OpenMP results exactly — the platform
  choice produced the behavior the profiling said to expect, with bit-identical results.
- **When the choice flips:** if the data no longer fit one machine's memory (or the course
  required distributed runs), MPI becomes the right answer — and at that point the paper's
  own data-distribution + locality-aware scheduling machinery (implemented in this project
  under `paper_strategy/`) is what addresses the remote-memory penalties. If a future
  kernel is dense and compute-bound (e.g. large GEMM at high intensity), CUDA becomes
  justifiable.

---

## 4. Reproducing every claim

```bash
make seq                                        # build the two sequential programs
./build/map_seq 48 131072 100                   # 0.356 s, checksum
./build/vecmul_seq 128 7168 1000                # 0.225 s, checksum
./scripts/profile.sh                            # perf hotspots + counters -> profiling/
./scripts/run_experiments.sh                    # thread sweep -> results/summary.md
./scripts/plot_results.py                       # results/speedup.png (+ checksum verification)
```
