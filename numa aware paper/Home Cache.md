# Home Cache

In manycore architectures like the [[TILEPro64 Architecture|TILEPro64]], a **Home Cache** is the specific L2 cache bank assigned to manage and track a particular cache line (e.g., 64 bytes) for the entire processor.

## Not Permanent DRAM
A crucial distinction from a [[UMA vs NUMA|NUMA node]] is that a home cache does **not** necessarily hold the *only* permanent copy of the data. Instead, it primarily participates in **cache coherence** and tracking/servicing that cache line across the distributed tile network. Other tiles and cores may possess cached copies of the data, but the home cache acts as the authoritative "manager" for that line.

## Mechanism

When a core needs a cache line and it isn't available locally, the request must traverse the on-chip network to reach the data's designated Home Cache:

```text
Core "I need cache line X"
 │
 ↓
Home Cache of X
 │
 ↓
Supplies cache line
```

## The Latency Penalty

The physical distance on the mesh network between the requesting core and the Home Cache heavily impacts performance.
* **Local Home Cache:** Fast, low latency.
* **Remote Home Cache (many tiles away):** Approximately 4–6× higher latency.

### Write-Through Behavior
On the TILEPro64, stores (writes) are typically **write-through to the Home Cache**. 
Because the Home Cache must remain updated to maintain coherence, it acts as a constant participant in the memory hierarchy. Even if a cache line is cached locally, modifications still incur network traffic to the Home Cache, making the physical location of the Home Cache critical.

## See Also
- [[NUMA vs TILEPro64 Architecture]]
- [[TILEPro64 Architecture]]
- [[Data Distribution on TILEPro64]]
