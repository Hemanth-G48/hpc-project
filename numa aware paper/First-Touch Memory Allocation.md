# First-Touch Memory Allocation

**First-Touch Memory Allocation** is a standard memory placement policy in [[UMA vs NUMA|NUMA]] systems where the operating system places a memory page on the physical NUMA node associated with the processor that *first accesses* (touches) it.

## Mechanism

When a program allocates memory (e.g., via `malloc`), physical memory isn't immediately mapped. The OS waits until a CPU actually reads from or writes to that memory space.

```text
CPU 0 first touches Page A
        ↓
Page A is allocated to Node 0

CPU 3 first touches Page B
        ↓
Page B is allocated to Node 3
```

## Performance Implications

While first-touch is an effective default heuristic, it can lead to severe bottlenecks in parallel applications if not managed carefully. For example, if a single master thread initializes all memory structures, all pages will end up on the master thread's NUMA node. When other threads subsequently attempt to access that memory, they will incur remote memory access penalties.

In experimental comparisons, explicitly distributing data across NUMA nodes (using [[Runtime-Assisted Data Distribution]] or tools like `numactl`) often yields better performance and fewer [[Dispatch Stall Cycles]] than relying purely on the OS first-touch policy.

## See Also
- [[Runtime-Assisted Data Distribution]]
- [[Data Distribution Policies]]
- [[UMA vs NUMA]]
