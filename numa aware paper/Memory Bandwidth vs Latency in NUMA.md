# Memory Bandwidth vs Latency in NUMA

In parallel computing on [[UMA vs NUMA|NUMA]] architectures, there is often a complex tradeoff between **memory latency** (how fast a single data access completes) and **aggregate memory bandwidth** (how much total data can be transferred simultaneously across the system).

## The Counterintuitive Relationship

When employing [[Runtime-Assisted Data Distribution]] to spread data evenly across all NUMA nodes, individual remote memory accesses might take longer (higher latency), leading to an increase in [[Dispatch Stall Cycles]]. However, the total execution time of the application can still *decrease*.

### Why this happens

If all memory is placed on a single node (e.g., via poorly managed [[First-Touch Memory Allocation]]), the latency for local CPUs is low, but the single memory controller quickly becomes a bottleneck:

```text
Node 0 memory controller
     ↑
Many CPUs competing
     ↑
Bandwidth bottleneck (Lower performance)
```

By explicitly distributing the data, traffic is spread across multiple memory controllers:

```text
Node 0     Node 1     Node 2
  ↑          ↑          ↑
CPU group  CPU group  CPU group
```

Even though an individual CPU might experience higher latency accessing remote data (causing more individual stalls), the **aggregate memory bandwidth** of the entire system is heavily utilized. 

```text
More individual stalls + Higher aggregate bandwidth = Lower total execution time
```

This demonstrates why performance metrics must be analyzed holistically; minimizing latency and stall cycles does not always yield the highest performance if bandwidth is the primary bottleneck.

## See Also
- [[Dispatch Stall Cycles]]
- [[Runtime-Assisted Data Distribution]]
- [[UMA vs NUMA]]
