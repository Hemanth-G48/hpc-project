# Data Locality in Task Scheduling

**Data Locality in Task Scheduling** refers to the principle that a task scheduler must consider not only how evenly computation is distributed ([[Load Balancing]]) but also how close each task is to the data it needs in memory.

## Load Balancing vs. Data Locality

In traditional scheduling, the primary focus is often purely on **load balancing**: ensuring every CPU has an equal share of the work to prevent idling. 

However, on modern multicore and [[UMA vs NUMA|NUMA]] systems, load balancing alone is insufficient. 
A perfect load balance (e.g., 50% work on CPU 0 and 50% work on CPU 1) can still result in poor performance if tasks are scheduled on processors that are distant from their data. 

* **Local memory access:** Faster and provides higher bandwidth.
* **Remote memory access:** Slower and potentially lower bandwidth.

### Example Scenario

Suppose there are two tasks:
* `Task A` primarily uses `Memory 0`.
* `Task B` primarily uses `Memory 1`.

If the workload shifts, a traditional scheduler that prioritizes only load balance might swap the tasks:
* `CPU 0` (close to `Memory 0`) gets `Task B`.
* `CPU 1` (close to `Memory 1`) gets `Task A`.

Now, both CPUs must frequently access remote memory. The scheduler must optimize for **both**:
1. **Load Balancing:** "Which CPU has the least work?"
2. **Data Locality:** "Which CPU is closest to the data this task needs?"

## Impact on Manycore Processors

This locality problem is not limited to distinct NUMA nodes. Inside a single manycore processor with **banked shared caches**, a core can access its nearby cache bank quickly, while accessing a distant bank takes longer. Thus, even within a single chip, data locality is critical for cache performance.

## See Also
- [[Load Balancing]]
- [[UMA vs NUMA]]
- [[Work Stealing]]
- [[Locality in HPC]]
