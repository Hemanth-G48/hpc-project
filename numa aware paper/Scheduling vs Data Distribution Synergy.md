# Scheduling vs Data Distribution Synergy

A core finding in locality-aware runtime research is that **Locality-aware scheduling is not automatically better than standard work stealing.** 

Data distribution determines *where the data lives*; scheduling determines *where the task runs*. When those two decisions agree with the application's memory access pattern, memory latency falls and performance improves. When they clash, a locality-aware scheduler might actually degrade performance by severely restricting load balancing without any actual locality to exploit.

## The 2x2 Experiment
Evaluating the cross-product of Distribution Policy (`Fine` vs `Coarse`) against Scheduling Policy (Standard [[Work Stealing]] vs [[Locality-Aware Task Scheduling]]) reveals critical workload dependencies.

### 1. The Map Pattern (1 Task : 1 Allocation)
In a Map pattern, Task 0 processes Vector A, Task 1 processes Vector B, etc.
* **Coarse + Locality-Aware (Best):** Vector A is placed entirely on Node 0. The scheduler places Task 0 on Node 0. This creates perfect, localized access, drastically reducing memory latency and execution time.
* **Fine + Locality-Aware (Worst):** Vector A is spread across *all* nodes. The scheduler finds no "best" node because the data is uniformly distributed (25% on each of 4 nodes). It defaults to the local queue, but its restricted stealing mechanism fails to load balance effectively compared to global work stealing. Thus, performance is *worse* than standard work stealing.

### 2. The Matmul Pattern (1 Task : Many Allocations)
In blocked matrix multiplication, a task updating a block of matrix $C$ must read blocks from $A$ and $B$.
* **Coarse + Locality-Aware (Good):** Even though blocks A, B, and C might be on different nodes, they are placed systematically. The scheduler calculates the lowest average access cost among the multiple blocks, yielding a measurable performance improvement over blind work stealing.
* **Fine (Bad for Matmul):** By spreading every block's pages across all nodes, every task sees a chaotic mix of local and remote accesses. There is no structured locality for the scheduler to exploit, and remote traffic floods the interconnect. In this scenario, Fine + Locality-Aware performs almost identically to Fine + Work-Stealing, and both are sub-optimal.

### 3. Vecmul (Load Balance Priority)
While Map tasks have strong 1:1 locality, vector multiplication tasks can benefit more from spreading work. A small vicinity tightly restricts stealing, leading to idle cores while others are overloaded. A larger vicinity restores load balancing, proving that **locality shouldn't always override load balance**.

### 4. Reduction (The Coarse Pitfall)
A Reduction workload that allocates a single, massive array via one `malloc` call exposes a critical flaw in naïve coarse distribution. The entire array is dumped onto a single [[Home Cache]]. The locality-aware scheduler correctly recognizes this and attempts to schedule *all* tasks on that single core, causing accidental serialization. A large vicinity is strictly required here to steal tasks away from the overloaded core and restore parallelism.

### 5. SparseLU (Complex Access Patterns)
SparseLU utilizes highly complex data access patterns that don't fit neatly into "spread everything" (Fine) or "keep allocation together" (Coarse). Performance is maintained, but not drastically improved, proving that simple distribution policies are not a silver bullet for every workload.

## The Timeline Evidence
Researchers often visualize this synergy using timeline graphs coloring task execution by memory access latency (e.g., Light Green = Local/Fast, Blue = Remote/Slow). 
These graphs empirically prove that **lower memory access latency directly correlates with shorter task execution time**.

## The Adaptive Default Scheduler
The ultimate conclusion of this research is that a locality-aware scheduler can safely serve as a system's default scheduler due to its **adaptive fallback behavior**. 

The runtime balances three conflicting priorities: *Data Locality*, *Load Balance*, and *Scheduling Overhead*.
1. If the underlying data distribution provides strong, structured locality (e.g., Coarse + Map), the scheduler aggressively exploits it for massive performance gains.
2. If the data provides no useful locality (e.g., Fine distribution or SparseLU), the scheduler recognizes the lack of localized advantage and automatically falls back toward a standard load-balancing [[Work Stealing]] approach, preventing significant performance degradation.

## Workload-Driven Decision Tree
The interaction between data distribution, scheduler locality, and work stealing limits (vicinities) can be visualized as a decision tree dictated entirely by the underlying workload:

```text
                    WORKLOAD
                       │
          ┌────────────┼─────────────┐
          ↓            ↓             ↓
         Map        Vecmul        Reduction
          │            │             │
     strong         load          one malloc
     locality      balance        allocation
          │            │             │
          ↓            ↓             ↓
      Coarse        Larger         Coarse
          │         vicinity          │
          ↓            │              ↓
     Locality ↑        │        Locality scheduler
          │            │        concentrates work
          ↓            ↓              │
     Small vicinity   Stealing        ↓
          │         important      imbalance
          ↓            │              ↓
       Faster      Larger          Stealing
                     vicinity       needed
```

## See Also
- [[Locality-Aware Task Scheduling]]
- [[Data Distribution Policies]]
- [[Work Stealing]]
