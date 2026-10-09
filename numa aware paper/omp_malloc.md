# omp_malloc

`omp_malloc` is a proposed portable allocation interface designed to expose [[Runtime-Assisted Data Distribution]] to OpenMP programmers without requiring machine-specific knowledge.

## Purpose

Instead of relying on hardware-specific APIs (like `libnuma` for NUMA nodes or specialized tile APIs for the [[TILEPro64 Architecture|TILEPro64]]), programmers use `omp_malloc` as a drop-in replacement for standard `malloc`.

```c
ptr = omp_malloc(size, hint);
```

The `hint` corresponds to [[Data Distribution Policies]] (e.g., Standard, Fine, Coarse).

## The Optimization Pipeline

`omp_malloc` performs two critical functions:
1. **Low-Overhead Wrapper:** It translates the high-level hint into the appropriate low-level hardware API, physically placing the data on the correct NUMA node or [[Home Cache]].
2. **Metadata Recording:** It actively records *where* the data was placed. This mapping of `Memory Region → Hardware Location` is the exact prerequisite that enables [[Locality-Aware Task Scheduling]].

## See Also
- [[Runtime-Assisted Data Distribution]]
- [[Data Distribution Policies]]
