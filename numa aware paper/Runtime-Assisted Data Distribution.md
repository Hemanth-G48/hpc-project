# Runtime-Assisted Data Distribution

**Runtime-Assisted Data Distribution** is a mechanism where programmers provide high-level hints to the runtime system regarding how memory should be placed, rather than relying on low-level, machine-specific APIs (see [[OpenMP Data Placement Limitations]]).

## The Mechanism: Allocation Hints

Instead of writing code that explicitly states, "Put these pages on NUMA node 2," the programmer uses an API similar to standard memory allocation, but augmented with a hint:

```c
// Standard allocation
ptr = malloc(size);

// Runtime-assisted allocation
ptr = omp_malloc(size, hint);
```

The `hint` tells the runtime about the *granularity* of distribution (e.g., using specific [[Data Distribution Policies]]). 

## Implementation and Overhead
The runtime does not reinvent memory management. It achieves low overhead by acting as a thin wrapper around existing, hardware-specific APIs:
* **On NUMA:** `omp_malloc` wraps `libnuma`.
* **On TILEPro64:** `omp_malloc` wraps the processor's specialized home-cache placement APIs.

The runtime merely performs a small amount of bookkeeping (e.g., tracking a round-robin counter for Coarse distribution) before delegating to the hardware.

## The Scheduling Pipeline
Crucially, data distribution is not an isolated feature. When `omp_malloc` places data, the runtime records *where* that data was placed (e.g., "Allocation A is at Node 2"). This mapping information is the direct prerequisite that enables [[Locality-Aware Task Scheduling]]. The scheduler queries this record to ensure tasks are executed on cores physically close to the data they need.

## Benefits

1. **Portability:** The abstraction is portable. A programmer asks, "How finely should my data be distributed?" and the runtime translates this to the appropriate hardware concept.
   * On a [[UMA vs NUMA|NUMA]] system, the distribution unit might be a memory page placed on a NUMA node.
   * On a manycore processor (like TILEPro64), the distribution unit might be a cache line placed in a home cache.
2. **Gradual Addition of Hints:** Programs without hints continue to run normally using standard OS placement. A programmer can incrementally add hints to specific data allocations to optimize performance without redesigning the entire application.

## The OpenMP Analogy

This philosophy directly mirrors how [[OpenMP]] handles CPU task scheduling. Programmers learn simple, high-level scheduling hints like:
```c
#pragma omp parallel for schedule(static)
#pragma omp parallel for schedule(dynamic)
```
They do not need to understand the underlying OS thread implementation. Similarly, this runtime provides `fine` or `coarse` as simple **data-distribution policies**, abstracting away the punishing complexity of NUMA/cache topologies.

## See Also
- [[Data Distribution Policies]]
- [[OpenMP Data Placement Limitations]]
