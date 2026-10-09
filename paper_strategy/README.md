# paper_strategy — the paper's strategy as a runnable mini-runtime

This folder implements the **parallelization approach of the course paper** (Muddukrishna,
Jonsson, Brorsson: *Locality-Aware Task Scheduling and Data Distribution for OpenMP Programs
on NUMA Systems and Manycore Processors*, Scientific Programming 2015) as a small
task-based runtime, and runs four of the paper's benchmarks through it:

| Driver | Paper benchmark | Data |
|---|---|---|
| `map_ps.c` | Map (1D vector scaling) | 63 vectors × 256 KB, 250 rounds |
| `vecmul_ps.c` | Vecmul (vector product) | 128 vector blocks × 112 KB × 3, 200 rounds |
| `reduction_ps.c` | Reduction (iterative merge) | 256 MB (2²⁵ doubles), depth 10 |
| `matmul_ps.c` | Matmul (blocked) | 1024², block 64 |

## Mechanism mapping

| Paper mechanism | Implementation here |
|---|---|
| `omp_malloc` + distribution policies (standard/fine/coarse; global + per-allocation control) | `ps_alloc(size, policy)`; coarse = whole allocation takes a round-robin home; fine/standard = treated as spread |
| Data-placement bookkeeping (the D distribution driving the scheduler) | allocation registry: each allocation (id) stores policy + home; tasks reference allocations by id |
| Task data footprint (paper: via OpenMP `depend` + access-intensity hint) | `ps_dep_t {aid, off, len, intensity}` — explicit footprint bytes + intensity hint |
| Work-dealing: enqueue to the least access-cost queue; O(N²) cost against the NUMA distance matrix | `ps_task()`: classify deps → skip to local queue if data is spread/standard or below `PS_CUTOFF` (the paper's L1/LLC cut-off); else min over locations of Σ bytes × distance; intensity hint jumps directly to that dependence's home |
| Work-stealing: idle threads steal by queue ranking; stealing thresholds; exponential back-off | `try_steal()`: ring-order victim scan, skip nearly-empty queues (`count <= 1`), spin-yield back-off |
| Vicinities (limit stealing to a neighbourhood) | `PS_VICINITY` = max ring distance for victim selection (0 = global) |
| Thread → core binding, one task queue per architectural location | one worker thread per location, sibling-aware pinning (physical cores first, then SMT siblings) |
| Trace/statistics | `PS_STATS=1` prints per-location executed/dealt/steals/foreign |

On this laptop (single NUMA node) the runtime emulates the paper's **manycore model** —
locations = cores, homes at cache level — while on a real multi-node machine the same code
path would use the OS distance matrix (`/sys/.../node/*/distance`) to target the memory-owning
node. The distance matrix here uses `10` (same location) / `21` (elsewhere, the paper's
max NUMA distance); where costs tie, the dealer prefers the dominant home — the
cache-level analogue of "the cheapest node".

## Build & run

```bash
make ps                                  # build all four drivers (root Makefile)

# knobs (environment):
#   PS_THREADS=12     worker threads / locations (default 12)
#   PS_LOCATIONS=12   number of queues; locations < threads groups workers
#   PS_DIST=coarse    ps_alloc policy: standard | fine | coarse
#   PS_SCHED=locality standard | locality  (work-dealing vs creator-queue)
#   PS_VICINITY=0     max steal ring distance (0 = global)
#   PS_CUTOFF=8192    footprint below this many bytes skips locality (paper: L1 size)
#   PS_STATS=1        print per-location scheduler statistics

PS_SCHED=locality PS_DIST=coarse ./build/map_ps 63 32768 250
```

Comparative study (ps vs plain OpenMP on identical kernels/sizes):

```bash
REPS=3 ./scripts/run_paper_strategy.sh   # -> results/paper_strategy/strategy.csv + stats.log
./scripts/plot_paper_strategy.py         # -> summary.md + strategy.png
```

## Verification

Every benchmark run prints a checksum; **all scheduler/policy/vicinity combinations reproduce
the sequential checksums exactly** (map/vecmul/reduction vs `sequential/` equivalents,
matmul vs `matmul_seq`), see `results/paper_strategy/summary.md`. The reduction driver also
verifies its output is fully sorted.

## Measured comparative results (12 threads, this laptop)

| benchmark | plain OpenMP | std + coarse | locality + coarse | loc/coarse vic=1 | loc/coarse vic=2 | locality + fine |
|---|---|---|---|---|---|---|
| map | 0.0404 | 0.0361 (0.90×) | 0.0389 (0.96×) | 0.0383 (0.95×) | 0.0353 (0.88×) | 0.0351 (0.87×) |
| vecmul | 0.1764 | 0.1542 (0.87×) | 0.1685 (0.96×) | 0.1680 (0.95×) | 0.1640 (0.93×) | 0.1696 (0.96×) |
| reduction | 0.1712 | 0.1879 (1.10×) | 0.1811 (1.06×) | **0.2422 (1.41×)** | 0.2060 (1.20×) | 0.1769 (1.03×) |
| matmul | 0.0474 | 0.0391 (0.82×) | 0.0407 (0.86×) | **0.1020 (2.15×)** | 0.0635 (1.34×) | 0.0419 (0.88×) |

Times in seconds; factors relative to plain OpenMP. All runs use pinned threads (physical
P-cores first), and repetitions are interleaved across configurations so transient slow
windows on the machine hit every strategy equally (best-of-5).

**What the numbers show**

1. **The vicinity mechanism reproduces the paper's confinement effect — on both kernels
   where dealing concentrates tasks.** `reduction` and `matmul` use one/few large
   allocations, so under `coarse` nearly all tasks are dealt to the same home queue; with
   vicinity = 1, stealing can only relay through ring neighbours → **+41 % (reduction) /
   +115 % (matmul)** execution time; vicinity = 2 partially recovers (+20 % / +34 %); global
   stealing fully rescues both (≈ standard). `stats.log` shows the mechanism directly:
   dealt concentrated at one location, steals cascading outward hop by hop.
2. **Balanced-home workloads are vicinity-insensitive**: `map` and `vecmul` allocate per
   vector, so homes spread across all locations and dealing is balanced — all vicinity
   cells sit within ~10 % of each other. The vicinity effect appears precisely when dealing
   concentrates work, not when it distributes it.
3. **Locality-aware scheduling is a safe default here**: within noise of the standard
   work-stealing scheduler on every benchmark — the paper's own "falls back to load
   balancing when locality is missing" claim, measured.
4. **Parity with plain OpenMP (0.82–1.10×)** once both sides use equal thread placement
   (`matmul` is 12–18 % faster under the runtime). An earlier unpinned comparison made the
   runtime look up to 1.9× faster on map; that gap was thread placement (unpinned OpenMP
   threads stalling on per-round barriers), not a runtime design advantage — a useful
   measurement lesson on hybrid CPUs.
5. Work-dealing behaviour is observable in stats: `loc/coarse` deals tasks to their homes
   (dealt ≈ executed, `foreign ≈ 0`), while `std` deals everything to one queue and lets
   stealing spread it.

## Interpretation & limitations (honest framing)

- On a **single-node UMA machine there is no remote-memory penalty to avoid**, so the
  paper's DRAM-level gains (e.g. 69 % on Vecmul) cannot materialize. The only residual
  locality is cache-level (per-core L2, shared between SMT siblings); a sweep across
  working-set sizes shows the window where homes fit L2 is exactly the window where the
  small rounds make runtime overheads dominant — so locality ≈ standard everywhere on this
  hardware. This is consistent with the paper's thesis rather than contradicting it:
  the benefit of locality-aware scheduling is bounded by how much locality the machine
  actually exposes.
- Sizes for map/vecmul are cache-scale (TILEPro64-style); reduction/matmul follow the
  paper's NUMA parameters.
- The `ps` runtime is a teaching-faithful subset of MIR, not MIR itself (no LIFO deques,
  no trace generation); its purpose is to make the strategy's mechanisms executable and
  measurable, not to replace the original implementation.
