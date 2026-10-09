# Locality-Aware Task Scheduling

**Locality-Aware Task Scheduling** is an advanced scheduling paradigm that determines where a task should execute by balancing traditional [[Load Balancing]] with [[Data Locality in Task Scheduling|Data Locality]].

## The Scheduling Decision

A traditional load-balancing scheduler simply asks:
> *"Which CPU has the least work?"*

A locality-aware scheduler expands this to ask:
> *"Which CPU has the least work?"* **AND** *"Where is the task's data located?"*

### Example

```text
             NUMA Node 0        NUMA Node 1

CPU 0 ─────── Memory A

CPU 1 ─────── Memory B
```

If Task `T1` needs to process data in `Memory A`:
* A **normal scheduler** might send `T1` to `CPU 1` if it is idle, resulting in slow remote memory accesses.
* A **locality-aware scheduler** utilizes the [[Task Data Footprint]] to recognize that `T1` accesses `Memory A`, which resides on `Node 0`. It will use a [[Work-Dealing]] algorithm to assign `T1` to `CPU 0`'s queue to enable faster, local memory access.

## Architecture-Bound Task Queues
To achieve this, the locality-aware scheduler binds task queues directly to physical architectural locations (e.g., `Queue 0 → NUMA Node 0`, `Queue 1 → NUMA Node 1`). Every CPU core processes tasks from its local architectural queue.

## Calculating the Placement Cost
When a task is created, the [[Work-Dealing]] mechanism computes an expected access cost for each possible architectural queue. 

It does this by taking the distribution $D$ of the task's footprint (e.g., 60% of data is on Node 0, 20% on Node 1, etc.) and weighting it against the [[NUMA Distance]] from a candidate node to all those data locations. The queue with the lowest overall calculated cost is selected to host the task.

### Opt-Outs and Calculation Shortcuts
Because evaluating the $D$ distribution against an $O(N^2)$ cost matrix is expensive, the scheduler utilizes several fast-path opt-outs to avoid unnecessary overhead:

1. **Cache Cutoffs:** The scheduler ignores data that easily fits within fast, private caches (e.g., L1 cache on manycore, LLC on NUMA). If the task's total working set `sum(D)` is smaller than this cutoff, locality scheduling is skipped entirely because the data will simply reside in the local cache.
2. **Distribution Policy Checks:** If the data is distributed via a `fine` policy, it is evenly spread across all nodes/tiles. Calculating the "best" node is futile because the access cost will be symmetric everywhere. The scheduler only performs rigorous locality checks if the data was allocated via a `coarse` policy.
3. **Access Intensity Hints:** A task might depend on Array A (read 1,000,000 times) and Array B (read 10 times). Calculating the weighted cost of both is wasteful. Programmers can provide an **Access Intensity** hint, allowing the scheduler to shortcut the math and simply place the task directly in the queue where Array A resides.

## Balancing Locality and Load

Locality-aware scheduling does not simply enforce strict data locality. If all CPUs on `Node 0` are fully busy, but CPUs on `Node 1` are idle, strictly waiting for `Node 0` would cause massive parallel overhead. 

The scheduler must dynamically weigh the cost of remote memory access against the cost of CPU idling, arriving at a final decision that optimizes overall execution time.

## The Synergy with Data Distribution

Locality-aware task scheduling and [[Runtime-Assisted Data Distribution]] are two complementary mechanisms that must work together. 

If data is distributed perfectly (e.g., `Data A` on `Node 0`), but the scheduler ignores this and places a task using `Data A` onto `Node 7`, the program will still suffer from remote memory access penalties. Conversely, if a locality-aware scheduler tries to run tasks near their data, but a poor [[First-Touch Memory Allocation]] placed *all* data on `Node 0`, then all tasks must execute on `Node 0`, destroying [[Load Balancing]].

Therefore:
```text
          Data
           ↓
   ┌─────────────────┐
   │ Good placement  │
   └────────┬────────┘
            ↓
       Locality info
            ↓
   ┌─────────────────┐
   │ Smart scheduler │
   └────────┬────────┘
            ↓
    Task near its data
            ↓
     Fewer remote accesses
            ↓
      Better performance
```

### Experimental Evidence: The Synergy
The effectiveness of this scheduler is entirely dependent on the underlying data distribution. If data is scattered uniformly (Fine distribution), locality-aware scheduling can actually perform *worse* than normal work stealing due to its restricted load-balancing capabilities. However, when paired with Coarse distribution on the right workload, it yields massive performance gains.

For a detailed breakdown of these experimental interactions (e.g., Map vs Matmul patterns), see [[Scheduling vs Data Distribution Synergy]].

## See Also
- [[Data Locality in Task Scheduling]]
- [[Task Data Footprint]]
- [[Runtime-Assisted Data Distribution]]
- [[Work Stealing]]
