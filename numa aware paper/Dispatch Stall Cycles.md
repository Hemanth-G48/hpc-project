# Dispatch Stall Cycles

**Dispatch Stall Cycles** are a performance metric that measures the amount of time a processor's execution is stalled while waiting for memory-related operations (such as load/store unit stalls) to complete.

## Role in Locality Evaluation

In [[UMA vs NUMA|NUMA]] and manycore architectures, dispatch stall cycles are heavily influenced by data locality. 

```text
CPU wants data
     ↓
Data is located in remote memory (high latency)
     ↓
Data isn't immediately available
     ↓
CPU waits
     ↓
Increase in Dispatch Stall Cycles
```

When evaluating [[Runtime-Assisted Data Distribution]], a decrease in dispatch stall cycles typically correlates with lower execution times because the CPU spends less time waiting for remote memory accesses. 

However, this metric must be evaluated holistically. In some scenarios, explicit data distribution might actually *increase* individual stall cycles, but because it improves aggregate bandwidth, the overall execution time still decreases (see [[Memory Bandwidth vs Latency in NUMA]]).

## See Also
- [[Memory Bandwidth vs Latency in NUMA]]
- [[Data Locality in Task Scheduling]]
