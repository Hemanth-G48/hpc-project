# paper_strategy_openmp — the paper's strategy expressed with stock OpenMP 5

This folder answers one question: **how much of the paper's strategy (locality-aware task
scheduling + data distribution) can be expressed with standard OpenMP**, without a custom
runtime? Four of the paper's benchmarks are implemented purely with OpenMP 5 clauses:

| File | Benchmark | OpenMP mechanisms used |
|---|---|---|
| `map_tasks.c` | Map | tasks + `depend` + `affinity`, one round of per-vector tasks per iteration |
| `vecmul_tasks.c` | Vecmul | tasks + `depend` (in: x,y / out: z) + `affinity` |
| `reduction_tasks.c` | Reduction | per-level merge tasks + `depend` + `affinity` |
| `matmul_tasks.c` | Matmul | blocked tasks + `depend` (footprint on A, B) + `affinity` on C block |
| `omp_paper.h` | — | `op_alloc` seam where data-distribution placement (libnuma / first-touch) would hook |

Build & run:

```bash
make tasks                                  # -> build/{map,vecmul,reduction,matmul}_tasks
OMP_NUM_THREADS=12 ./build/map_tasks 63 32768 250
REPS=5 ./scripts/run_paper_strategy_openmp.sh   # -> results/paper_strategy_openmp/tasks.csv
```

Thread placement follows the project convention (`taskset` to physical P-cores first);
the equivalent native form is
`OMP_PLACES="{0},{2},{4},{6},{8},{10},{1},{3},{5},{7},{9},{11}" OMP_PROC_BIND=close`.

## Capability mapping (the honest answer)

| Paper mechanism | Closest OpenMP 5 equivalent | Expressed? |
|---|---|---|
| Task data footprint via `depend` clauses | `#pragma omp task depend(in: ...)` — the paper itself used this | **yes, native** |
| Data distribution standard/fine/coarse (`omp_malloc`) | *nothing* — OpenMP has no memory-placement API; needs libnuma (`mbind`/`numa_alloc_interleaved`) or first-touch init in a C helper | **no** (OS/runtime territory) |
| Access-intensity hint | no equivalent | **no** |
| Locality-aware placement (work-dealing by cost matrix) | `affinity(region)` hint + `OMP_PLACES`/`OMP_PROC_BIND` — the *runtime* decides placement; you cannot steer it per task by a cost model | **partial (hint only)** |
| Task queues bound to architectural locations | places exist (`OMP_PLACES`) but tasks are not mapped to place-bound queues | **no** |
| Vicinity-limited stealing | the runtime's stealing policy is not exposed | **no** |
| Steal thresholds / back-off | only coarse `OMP_WAIT_POLICY` | **no** |

So: the **task/dataflow half** of the paper's idea has a native OpenMP home (OpenMP 5.0
standardized the task `affinity` clause years after this paper — the standardized
descendant of exactly this research line), while the **scheduler-steering half**
(cost-based dealing, vicinities, location-bound queues) remains custom-runtime territory.

## Verification

All four task-based programs reproduce the **sequential checksums exactly** (map/vecmul/
reduction vs `sequential/`; matmul vs `matmul_seq`) — the `depend` regions preserve the
per-element operation order across rounds/levels, just like the intent of the paper's
`depend`-based footprint estimation. `scripts/run_paper_strategy_openmp.sh` fails loudly on
any mismatch.

## Measured results (12 threads, pinned, best-of-5)

| benchmark | loop OpenMP | **OpenMP 5 tasks (depend + affinity)** | ps runtime std/coarse | ps runtime loc/coarse |
|---|---|---|---|---|
| map | 0.0404 | **0.0292** | 0.0361 | 0.0389 |
| vecmul | 0.1764 | **0.1536** | 0.1542 | 0.1685 |
| reduction | 0.1712 | **0.1670** | 0.1879 | 0.1811 |
| matmul | 0.0474 | **0.0390** | 0.0391 | 0.0407 |

(loop-OpenMP and `ps` columns from `results/paper_strategy/`; times in seconds.)

**What this shows**

1. At these benchmark sizes, the OpenMP 5 task versions are **competitive with or faster
   than every other strategy tested** — including the custom runtime (`ps`) and the loop
   parallel versions. The task model pipelines rounds well (one parallel region, work is
   taken as it is created; `taskwait` only where a round/level must complete).
2. Losing the scheduler steering costs measurable behaviour elsewhere: the paper's hallmark
   effects (reduction's coarse-confinement serialization, matmul's +115 % small-vicinity
   penalty in `../paper_strategy/`) are **invisible and uncontrollable** here — stock
   OpenMP owns those decisions.
3. On a single-node machine there is no remote-memory penalty to steer around, so the
   missing machinery costs nothing measurable here; on a real multi-node NUMA system the
   `affinity` hint is what stock OpenMP offers, and anything more precise (per-task
   cost-based dealing, vicinity limits) again requires a custom runtime — which is exactly
   why the paper built one.
