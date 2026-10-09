# Related Work - Design Inspirations

**Continued from [[Related Work]]** — after surveying the broad landscape, the authors narrow the discussion to their exact design choices: the works that directly inspired **specific pieces of their system**, and especially the few works targeting **manycore processors** like [[TILEPro64 Architecture|TILEPro64]].

The way to read this section: *"Here are the papers that directly inspired different pieces of our system."*

## General Design Inspirations

### Memphis — Diagnosing NUMA Problems

Memphis is mainly a **NUMA performance diagnosis tool**. It monitors hardware performance counters:

* **QPI / crossbar activity** → how much traffic flows between processors
* **LLC misses** → how often data is missing from the last-level cache

```text
Program running
      ↓
Hardware performance counters
      ↓
Measure: remote/crossbar traffic + cache misses
      ↓
Detect NUMA problems
      ↓
Tell the programmer what to fix
```

Example finding: a Node 0 CPU generating lots of remote traffic to Node 1 memory → recommendations like *"pin these threads differently"*, *"change how memory is distributed"*, or *"keep the computation's structure consistent"*.

**Relevance:** Memphis inspired **how the authors' scheduler works and how they evaluate it**. It is not their scheduler.

### Liu & Mellor-Crummey — HPCToolkit

They added NUMA-related analysis to **HPCToolkit**, and report an important finding:

> **Coarse / block-wise data distribution can outperform the default policy.**

```text
Default / first-touch:   pages end up wherever they are first touched
Coarse:                  Array A → Node 0, Array B → Node 1, Array C → Node 2
```

This supports a central idea of the paper: **data placement itself can significantly affect performance**. HPCToolkit is multi-architecture, so the authors see it as a useful foundation for implementing more advanced distribution policies. (See [[First-Touch Memory Allocation]].)

### Schmidl et al. — Where `fine` and `coarse` Come From

The names **fine** and **coarse** are inspired by Schmidl et al.'s `scatter` and `compact` — which were proposed for **thread placement**, not data distribution.

```text
Node 0: C0 C1 C2 C3      Node 1: C4 C5 C6 C7

Scatter: spread threads apart   → C0  C2  C4  C6   (more memory resources)
Compact: keep threads close     → C0 C1 C2 C3      (better locality)
```

| Schmidl et al. | This Paper |
|:--|:--|
| `scatter` / `compact` | inspired the naming |
| thread placement | applies the idea to **data distribution** |
| — | `fine` → spread data units |
| — | `coarse` → keep allocations together |

Don't confuse the two: same inspiration, different target. (See [[Data Distribution Policies]].)

### Task & Data Affinity

> *"Task and data affinity mechanisms discussed in our work are greatly inspired by the large body of research on NUMA optimizations for OpenMP runtime systems."*

Affinity means: **keep something close to what it frequently interacts with.**

```text
Task A — mostly accesses data on Node 2
        ↓
Task A → execute on Node 2       (task-data affinity)

Instead of:   Task A → random CPU → remote data
```

### Broquedis et al. — Hiding Hardware Details

The authors' **implicit memory allocation and architecture-locality-based scheduling** was inspired by Broquedis et al. (whose thread-migration idea also appears in [[Related Work]]).

Their runtime tries to hide hardware details. The programmer simply writes `omp_malloc(...)` and picks something like `fine` or `coarse`, while the runtime knows about NUMA nodes, [[NUMA Distance|distances]], memory placement, and task queues — so the programmer doesn't manage those details manually. (See [[Runtime-Assisted Data Distribution]].)

## Manycore / TILEPro64

> *"Few works have tackled data distribution and locality-aware scheduling on manycore processors."*

```text
NUMA system:    multiple processors + memory nodes
TILEPro64:      many cores + banked shared caches
```

There is much more research for traditional NUMA machines than for manycore processors — so the following works are the few closest to this paper's TILEPro64 work.

### Yoo et al. — Locality-Aware Scheduling for Manycore

One of the most important related works: locality-aware scheduling for data-parallel programs on manycore processors. Their finding matches this paper's:

> **Normal work stealing doesn't understand locality.**

```text
              Shared cache banks
      C0       C1       C2       C3
      |        |        |        |
      A        B        C        D

Task T mainly needs data A:

Locality-aware:   T → C0 → local data A
Work stealing:    T → C3 → remote access to A     (correct, but slower)
```

This is exactly what the paper demonstrates with the **Map** pattern. (See [[Scheduling vs Data Distribution Synergy]].)

### Yoo et al.'s Solution — and Its Catch

Their scheduler maximizes the probability that **the entire memory footprint of a group of tasks fits into the lowest-level cache that can hold it**:

```text
Task group: T1→A, T2→B, T3→C
Combined footprint: A+B+C = 200 KB
L2 cache = 256 KB   →  place the group so A+B+C share that cache
```

But it requires a lot of information:

```text
Task grouping + task ordering + read/write sets + profiling + offline graph analysis
```

The paper wants something **lighter weight**, able to use information already available through OpenMP task dependencies (`depend`) and runtime data-distribution information:

```text
Yoo et al.:   more sophisticated → potentially better optimization → more info/analysis required
This paper:   simpler → less programmer/profiling effort → runtime decides dynamically
```

### Vikranth et al. — Restricted Stealing

Directly related to the paper's **vicinity** concept: restrict stealing to groups of cores based on processor topology.

```text
Group 0: C0 C1 C2 C3     Group 1: C4 C5 C6 C7     Group 2: C8 ...

A thread can steal within its own group — not arbitrarily from every core.
```

```text
Vikranth et al.:   topology-based restricted stealing
This paper:        vicinity-based restricted stealing
```

(See [[Work Stealing]] and [[Work-Dealing]].)

### Tousimojarad & Vanderbauwhede — Data Copies

A TILEPro64 technique. When data is uniformly distributed across [[Home Cache|home caches]], a thread may need a remote bank:

```text
Core 0 ─────────→ Home Cache 3 (holds D)      — remote
```

Their solution: **make copies of data whose home cache is local to the accessing thread** → lower latency.

**Why this paper doesn't do that:** it targets standard OpenMP programs written in C, which makes **object migration** difficult — a runtime would need object semantics that plain C simply doesn't provide.

### Zhou & Demsky — Adaptive Garbage Collector

Their system runs in a **managed / garbage-collected** environment, so the GC can identify objects and migrate them toward the nodes that access them frequently:

```text
Object A — frequently accessed by Node 0  →  migrate to Node 0
```

Again, this paper targets **standard C/OpenMP** — there is no garbage collector that "knows" a memory object can be safely moved, so object migration is much harder.

## Compile-Time Data Layout

Instead of changing placement dynamically, these approaches optimize it at compile time.

### Lu et al.

Rearrange **affine loops** during compilation so accesses better match the distributed cache layout:

```text
Core → nearby cache bank → data       (after rearrangement)
instead of
Core → remote cache bank → data
```

### Marongiu & Benini

Extended OpenMP with interfaces for **array partitioning**; the compiler then distributes arrays based on profiled access patterns:

```text
OpenMP program → profile access pattern → compiler → partition array → distribute across banks
```

Similar to this paper's data-distribution idea, but motivated differently: they want data distribution on **MPSoCs without hardware memory-management support**.

### Li et al.

Use **compile-time information** to help the runtime decide data placement — a hybrid sitting between pure compiler optimization and pure runtime optimization:

```text
Compiler analyzes program → gives info to runtime → runtime chooses placement
```

### R-NUCA — Non-Uniform Cache Architecture

The final piece: hardware/cache-level page migration.

```text
       Shared cache banks
   C0  C1  C2  C3
    \  |   |  /
      network

Not every bank is equally close to every core:
Core 0 → Cache 0 = fast        Core 0 → Cache 3 = slower
```

R-NUCA automatically **migrates shared-memory pages into shared cache memory**, moving data toward its users to reduce cache access latency.

## The Entire Section in One Picture

```text
                  LOCALITY PROBLEM
                         │
        ┌────────────────┴────────────────┐
        │                                 │
    DATA PLACEMENT                    SCHEDULING
        │                                 │
   ┌────┴────┐                       ┌────┴────┐
   │         │                       │         │
Manual    Automatic              Locality   Restricted
   │       migration             aware      stealing
   │         │                    │            │
Huang     Memphis                Yoo        Vikranth
Minas     Page migration         MTS        This paper's
Majo      Replication            Charm++    vicinity
   │
   │
Compile-time
   │
Lu / Marongiu / Li
   │
   ↓
Manycore/TILEPro64
```

## This Paper's Contribution

```text
             THIS PAPER
                  │
       ┌──────────┴──────────┐
       │                     │
 Data distribution      Task scheduling
       │                     │
  fine / coarse        locality-aware
       │                     │
       └──────────┬──────────┘
                  │
           + work stealing
                  │
          + vicinity control
                  │
                  ↓
       Good locality + load balance
```

## The Key Thing to Understand

> The authors aren't claiming they invented every individual technique. They **combine ideas from several research directions** into a relatively simple OpenMP runtime.

> **Use simple `fine`/`coarse` data-distribution policies to know where data lives, then use task footprint + hardware locality information to decide where tasks should execute, while using work stealing / vicinities to recover load balance.**

That's why this section is so useful: almost every paragraph explains **where one piece of their final design came from**.

## See Also
- [[Related Work]]
- [[Runtime-Assisted Data Distribution]]
- [[Data Distribution Policies]]
- [[Work Stealing]]
- [[Work-Dealing]]
- [[Task Data Footprint]]
- [[Home Cache]]
- [[TILEPro64 Architecture]]
- [[NUMA Distance]]
- [[Scheduling vs Data Distribution Synergy]]
