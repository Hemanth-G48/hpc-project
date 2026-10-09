# Related Work

How prior work attacked the [[Data Locality in Task Scheduling|NUMA locality problem]], and where this paper positions itself. The prior art splits into two broad families — and the paper's contribution is essentially **combining** them:

1. **Data distribution / page migration** — control *where the data lives*.
2. **Locality-aware task scheduling** — control *where the task executes*.

The fundamental problem both families attack:

```text
NUMA machine

Node 0                 Node 1
CPU + RAM              CPU + RAM
   |                       |
   +------ network --------+
```

A task running on Node 0 that touches data on Node 1 pays the remote-access penalty:

```text
Task on Node 0
      |
      | remote access
      ↓
Data on Node 1
```

## 1. Data Distribution Approaches

### Huang et al. — Explicit OpenMP Placement

Extended OpenMP so programmers could explicitly say where data should go. Main approach: **block-wise distribution**, i.e. the granularity of the paper's [[Data Distribution Policies|coarse policy]]:

```text
Array A:
[A0][A1][A2][A3][A4][A5][A6][A7]

Node 0 → A0 A1
Node 1 → A2 A3
Node 2 → A4 A5
Node 3 → A6 A7
```

* **Strength:** Precise, programmer-directed control.
* **Cost:** Requires compiler/OpenMP specification support, and programmers must explicitly understand the placement locations → **more control, more programming effort**.

The paper wants something simpler.

### Minas — Page-Level Placement API

Provides an API that can place *specific memory pages* on *specific NUMA nodes*:

```text
Page 0 → Node 0
Page 1 → Node 0
Page 2 → Node 2
Page 3 → Node 3
```

The placement mapping is decided by **profiling + automatic code transformation**:

```text
Program → profile → observe memory accesses → determine placement → transform → rerun
```

* **Strength:** Very powerful fine-grained control.
* **Cost:** The system becomes quite complicated — precise control is powerful, but requires expert programmers and sophisticated transformation machinery.

### Majo & Gross — Dynamic Iteration-Level Distribution

Monitors how loops access memory, then redistributes pages *between loop iterations* so that every iteration mostly accesses local memory:

```text
Iteration 1: placement A → redistribution → Iteration 2: placement B → redistribution → ...
```

* **Strength:** Can give excellent locality when access patterns are stable.
* **Cost:** Distribution is dynamic, and **moving pages costs time**.

### Dynamic Page Migration (Automatic)

Instead of deciding placement beforehand, **watch the program while it runs** and move pages toward their heaviest user:

```text
Before: Node 0 keeps accessing → Page X (on Node 1)
After:  Page X migrated → Node 0 → local accesses
```

* **Nikolopoulos et al.** — pioneering *user-level* framework: continuously trace page accesses, find "hot" pages, find which node accesses them most, and migrate the page closer to that node. Attractive because the programmer specifies nothing.
* **Terboven et al.** — *next-touch* migration on Linux: instead of only looking at past accesses, predict **where the page is likely to be touched next** and migrate it toward that node.
* **Broquedis et al.** — instead of moving data to the thread, **move the thread closer to the data**. This is very closely related to this paper's philosophy.
* **Carrefour** — tackles **traffic congestion** on the interconnect: can both migrate *and* **replicate** pages so many readers hit local copies instead of all going to one node. Importantly: applications don't need to be modified.

* **Strength:** No effort from the programmer.
* **Cost:** See below — the paper argues this is a double-edged sword.

### The Double-Edged Sword of Automation

The paper points out a major problem with automatic migration:

> *Dynamic page migration requires no effort from the programmer, which is a double-edged sword.*

At first this sounds contradictory — why is *no effort* bad? Because when performance changes, it becomes difficult to understand **why**:

```text
Program → OS/runtime automatically moves pages → performance changes

Is the algorithm bad?
Is the data placement bad?
Did pages migrate?
Did the input change?
Did migration overhead increase?
```

Automatic optimization can become a **black box**. Worse, **input changes silently change performance**: the same program with a different input shows a different access pattern, pages migrate differently, and the runtime's decisions change — with no source-code change at all.

This is a key reason the authors prefer:

```text
Programmer specifies simple distribution policy
                +
Runtime handles hardware details
                +
Locality-aware scheduler
```

rather than completely automatic hidden migration.

## 2. Locality-Aware Task Scheduling

The complementary question: **where should a task execute?** (Data distribution asks "where is the data?" — this paper combines both.)

### Locality Domains (Manual)

Researchers created **locality domains** — topology "boxes" to which tasks are manually assigned and then scheduled within:

```text
Domain 0: {Task A, Task B}      Domain 1: {Task C, Task D}
```

* **Cost:** Again programmer effort — the programmer must understand the machine's locality structure.

### MTS — Socket-Hierarchy Queues

MTS organizes scheduling around the **socket hierarchy**, with a task queue per socket:

```text
Socket 0 → Queue 0      Socket 1 → Queue 1
```

This is **similar to this paper's task queue per NUMA node**. The difference is in stealing: MTS allows only **one idle core per socket** to steal bulk work from another socket, because unrestricted stealing causes lots of remote accesses.

```text
More stealing → better load balance → potentially worse locality
```

This is exactly the trade-off this paper's **vicinity** mechanism addresses (see [[Work Stealing]] and [[Work-Dealing]]).

### Charm++ — Communication-Aware Placement

Uses the NUMA topology **plus task communication information** to place tasks that communicate heavily close together:

```text
Task A ↔ (heavy communication) ↔ Task B
        → co-located on the same node
```

So Charm++ cares about **communication locality**, not just memory locality.

### Chen et al. — Cache-Pollution-Aware Partitioning

When a core steals a task from another socket, that task can flood the core's cache with remote data and evict the original working set, causing extra misses when the original work resumes. Chen et al. use **memory-access-aware task-graph partitioning** to reduce this cache pollution.

## 3. Where This Paper Sits

The authors position their work **between two extremes**:

```text
Manual precise control:        Programmer specifies exact page/node placement
                               → excellent control, high programming effort

Fully automatic migration:     Runtime/OS monitors accesses and moves pages/threads
                               → low programming effort, harder to understand/debug
```

| | Manual Precise Control | Fully Automatic Migration | **This Paper** |
|:--|:--|:--|:--|
| Who decides placement? | Programmer | Runtime / OS | Programmer picks a simple policy; runtime handles hardware detail |
| Effort | High | None | Low |
| Explainability | High | Black box | High |
| Examples | Huang, Minas | Nikolopoulos, Terboven | Fine/Coarse + locality-aware scheduling |

Their approach:

```text
Programmer → simple policy (fine / coarse)
      ↓
Runtime understands hardware → distributes data
      ↓
Locality-aware scheduler → places tasks near data
```

### Key difference from page migration

```text
Page migration:  running program → observe accesses → move pages → hope future accesses are local

This paper:      choose distribution → know data distribution → estimate task footprint
                 → choose execution location
```

Instead of constantly **moving data toward threads**, the paper often **assigns tasks toward data** — avoiding the cost and unpredictability of repeated page migration. (See [[Task Data Footprint]] and [[Work-Dealing]].)

## The Big Picture

```text
                 NUMA locality problem
                         │
             ┌───────────┴───────────┐
             │                       │
       DATA PLACEMENT            TASK PLACEMENT
             │                       │
      ┌──────┴──────┐         ┌──────┴──────┐
      │             │         │             │
   Manual        Automatic   Manual      Locality-aware
      │             │         │             │
 Huang/Minas    Migration   Locality     MTS/Charm++
 Majo & Gross   Replication domains       Chen et al.
                     │
                     │
              Low programming effort
                     │
              but harder to debug
                     │
                     ↓
              ┌───────────────┐
              │ THIS PAPER    │
              │               │
              │ Simple data   │
              │ distribution  │
              │      +        │
              │ locality-aware│
              │ scheduling    │
              └───────────────┘
```

## Main Takeaway

> The authors aren't claiming nobody has thought about NUMA locality. Existing solutions force a trade-off between precise control, programming effort, runtime complexity, and scheduling locality — and this paper tries to get good locality behind a **simple programmer interface** by combining fine/coarse data distribution with locality-aware task scheduling.

## See Also
- [[Related Work - Design Inspirations]] (continuation — where each design piece came from)
- [[Runtime-Assisted Data Distribution]]
- [[Data Distribution Policies]]
- [[Locality-Aware Task Scheduling]]
- [[Work Stealing]]
- [[Work-Dealing]]
- [[Task Data Footprint]]
- [[Scheduling vs Data Distribution Synergy]]
- [[OpenMP Data Placement Limitations]]
- [[First-Touch Memory Allocation]]
