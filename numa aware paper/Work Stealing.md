# Work Stealing

**Work Stealing** is a scheduling strategy in parallel computing where an idle processor (or thread) attempts to "steal" computing tasks from the queue of a busy processor to keep itself utilized.

## Mechanism

In a work-stealing scheduler, each processor maintains its own local queue of tasks. 
* As long as a processor has tasks in its queue, it executes them.
* When a processor finishes all tasks in its local queue and becomes idle, it looks at the queues of other processors and "steals" tasks from them.

```text
CPU 0: Task Task Task Task
CPU 1: Task
CPU 2: Task Task
CPU 3: Task Task Task
             ↓
CPU 1 finishes early, steals a Task from CPU 0
```

## Benefits and Drawbacks on NUMA Systems

The primary benefit of work stealing is improved **[[Load Balancing]]**. It dynamically shifts work from overloaded processors to idle ones, minimizing wait times.

However, on modern multicore and [[UMA vs NUMA|NUMA]] architectures, work stealing can cause severe performance issues due to poor **[[Data Locality in Task Scheduling|Data Locality]]**.

### The NUMA Penalty

1. `CPU 0` owns and accesses data located in its local memory (`Memory 0`).
2. `CPU 3` finishes its work early and steals a task from `CPU 0`.
3. `CPU 3` now executes the task, but the data remains in `Memory 0`.
4. `CPU 3` must perform **remote memory accesses** to read and write the data.

## Work Stealing in Locality-Aware Schedulers

Because work stealing is a highly realistic and common baseline strategy, modern [[Locality-Aware Task Scheduling]] research often builds upon it to maintain [[Load Balancing]] when strict locality fails. 

When a thread on Node 3 becomes idle, instead of stealing randomly, it executes a **locality-aware steal**:
1. **Queue Ranking:** It looks at all available architectural queues and ranks them by [[NUMA Distance]]. Node 3 will attempt to steal from nearby Node 2 before attempting to steal from distant Node 0.
2. **Stealing Thresholds:** It will not steal from a queue that is nearly empty. Stealing the last task from a neighbor just causes that neighbor to become idle and trigger its own steal, creating an expensive chain reaction.
3. **Exponential Back-Off:** If an idle thread continually polls and finds no tasks to steal, it utilizes exponential back-off (waiting 1 cycle, then 2, then 4, etc.) to prevent wasting CPU cycles and hammering the interconnect when there is simply no parallel work available.

## Vicinities in Manycore Stealing
On manycore processors like the [[TILEPro64 Architecture|TILEPro64]], queue ranking is further restricted by **Vicinities**. 

A vicinity defines the strict neighborhood bounds from which an idle thread is allowed to steal. Allowing unlimited, global stealing on a 64-core grid ensures perfect load balancing but can trigger disastrous locality if a task is stolen from across the chip.

The vicinity size acts as a configurable slider trading off locality and load balance:
* **Vicinity = 1:** No stealing allowed. Maximum locality, terrible load balancing.
* **Vicinity = 4:** Thread can steal from 3 immediate neighbors. Good locality, okay load balancing.
* **Vicinity = 63:** Thread can steal from anywhere on the chip. Terrible locality, perfect load balancing.

## See Also
- [[Load Balancing]]
- [[Data Locality in Task Scheduling]]
- [[UMA vs NUMA]]
