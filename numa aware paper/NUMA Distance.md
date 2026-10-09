# NUMA Distance

**NUMA Distance** is a topology-based metric reported by the operating system that approximates the relative cost of communication and memory access between different [[UMA vs NUMA|NUMA]] nodes.

## Concept

NUMA distance is not a direct measurement of time (e.g., "access takes exactly 22 nanoseconds"). Instead, it represents a relative penalty for accessing distant memory. 

```text
Node A → nearby Node B
      (Low NUMA distance = relatively cheap)

Node A → far-away Node H
      (High NUMA distance = more expensive)
```

Because modern NUMA topologies can be complex (e.g., 8 nodes connected via various hops), NUMA distance provides a [[Locality-Aware Task Scheduling|Locality-Aware Task Scheduler]] with a heuristic to understand which remote accesses will be the most detrimental to performance. 

However, to understand the true hardware access costs, actual memory latency should be measured empirically using benchmarking tools (like BenchIT).

## See Also
- [[UMA vs NUMA]]
- [[Locality-Aware Task Scheduling]]
- [[First-Touch Memory Allocation]]
