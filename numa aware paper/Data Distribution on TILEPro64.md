# Data Distribution on TILEPro64

Similar to multi-node NUMA systems, the [[TILEPro64 Architecture|TILEPro64]] allows programmers to control where data is placed across its grid of [[Home Cache|Home Caches]].

## 1. Uniform Distribution

By default, standard `malloc` allocations on the TILEPro64 are **uniformly distributed**. Cache lines from a memory page are spread round-robin across all home caches in the system.

```text
Page Allocation A
 │
 ├── Line 0 → Home 0
 ├── Line 1 → Home 1
 ├── Line 2 → Home 2
 └── ...
```
While this is excellent for spreading memory traffic and preventing bandwidth bottlenecks (see [[Memory Bandwidth vs Latency in NUMA]]), it destroys data locality for specific tasks. If a task needs sequential data from Allocation A, it will be forced to fetch cache lines from across the entire processor.

## 2. Single-Home-Cache Distribution

Alternatively, data can be explicitly associated with a single, specific Home Cache:

```text
Allocation A → Home Cache 10
Allocation B → Home Cache 30
```
This strategy allows the programmer to intentionally create data locality. When combined with [[Locality-Aware Task Scheduling]], the scheduler can place the task that processes `Allocation A` on a core physically close to `Home Cache 10`, drastically reducing remote access latency.

## Migration
The Home Cache of an allocated line can be changed later through **migration**, but this is an extremely expensive operation. It is generally far more performant to place the data correctly upon initial allocation using [[Runtime-Assisted Data Distribution]] hints.

## See Also
- [[Home Cache]]
- [[TILEPro64 Architecture]]
- [[Locality-Aware Task Scheduling]]
