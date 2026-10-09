# Task Data Footprint

A **Task Data Footprint** is the explicit information provided by a programmer (or inferred by a compiler/runtime) about which specific data structures or memory regions a task will access during its execution.

## Role in Scheduling

This footprint is a critical input for [[Locality-Aware Task Scheduling]]. 

When using [[Runtime-Assisted Data Distribution]], the runtime system knows exactly where data (e.g., `Data X` and `Data Y`) is physically located across various NUMA nodes. However, the runtime also needs to know *which tasks need that data*. 

By defining a task data footprint:
```text
Task A → {X, Y}
Task B → {Z}
Task C → {X}
```

The scheduler can match the task's required footprint against the known physical locations of the data to intelligently select an execution CPU that minimizes remote memory access costs.

## Implementation via OpenMP `depend` Clauses

Because OpenMP 4.0 lacked a dedicated task-footprint definition mechanism, locality-aware runtimes often infer the footprint using the existing `depend` clause. 

```c
#pragma omp task depend(in: A[0:100])
```
This clause signals that the task requires elements `A[0:100]`. The runtime intercepts this to estimate the footprint.

### The "Fragile Estimate" Problem
Estimating the footprint via `depend` clauses is considered **fragile** because the optimization completely depends on the programmer giving accurate dependency information.

If a task actually accesses 1000 elements (`A[0:1000]`) but the programmer only listed `depend(in: A[0:100])`, the scheduler is blind to the other 900 elements:

```text
What the scheduler thinks the task uses:
A[0 ───────── 99]

Reality:
A[0 ─────────────────────────────── 999]
```

This creates two divergent outcomes based on programmer accuracy:

```text
Correct depend information
        ↓
Correct data footprint
        ↓
Good locality decision
```
*versus:*
```text
Incomplete depend information
        ↓
Wrong data footprint
        ↓
Bad locality decision
```

To mitigate this fragility, programmers must rigorously use **array sections** (like `A[0:1000]`) to describe the full, comprehensive bounds of the data the task actually uses, ensuring the scheduler has complete information to make its placement decision.

## See Also
- [[Locality-Aware Task Scheduling]]
- [[Runtime-Assisted Data Distribution]]
