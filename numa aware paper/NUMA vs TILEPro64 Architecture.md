# NUMA vs TILEPro64 Architecture

When evaluating [[Data Locality in Task Scheduling]], the fundamental difference between traditional [[UMA vs NUMA|NUMA]] systems and manycore processors like the [[TILEPro64 Architecture|TILEPro64]] lies in **where the data is distributed and how processors access it**. 

Both exhibit variable local vs. remote access costs, but the **granularity** and underlying **hardware mechanisms** differ significantly.

## High-Level Comparison

| Feature | NUMA system | TILEPro64 |
| :--- | :--- | :--- |
| **Architecture** | Multiple CPU sockets/nodes + memory | 64 CPU cores arranged as a 2D mesh |
| **Data distribution** | **Memory pages → NUMA nodes** | **Cache lines → home caches/tiles** |
| **Main memory** | Physically divided among NUMA nodes | Shared external memory, with distributed caches |
| **Unit of locality** | Usually a **page** (e.g., 4 KB) | **Cache line** (e.g., 64 B) |
| **Local access** | CPU accesses memory attached to its NUMA node | Core accesses data through nearby/distributed cache/network |
| **Remote access** | Accessing another node's DRAM is slower | Accessing another tile's cache/home is slower |
| **Communication** | NUMA interconnect | 2D on-chip mesh network |
| **Main optimization** | Put frequently accessed pages near the CPUs using them | Distribute cache-line homes to reduce mesh congestion/latency |

## The Biggest Conceptual Difference

> **NUMA is primarily a distributed-memory locality problem (Large-grain), while TILEPro64 is a distributed-cache/on-chip-network locality problem (Fine-grain).**

### 1. NUMA (Large-grain locality)
```text
     PAGE
       ↓
┌───────────┐
│ NUMA Node │
└───────────┘
       ↓
   DRAM access
```
The locality unit is an entire memory page. If CPU 0 accesses its own Node 0 memory, it is fast. If it accesses Node 1 memory, the request must cross the NUMA interconnect, which is much slower.

### 2. TILEPro64 (Fine-grain locality)
```text
   CACHE LINE
       ↓
┌────────────┐
│ Home Tile  │
└────────────┘
       ↓
Distributed cache / mesh
```
The locality unit is a small cache line. Each memory address is mapped to a specific [[Home Cache]] tile on the mesh (e.g., `Address A → Home Tile 37`). A core accessing data located at a distant Home Tile will incur significant latency as the request routes through the 2D on-chip mesh network.

## The Mental Model (Library vs Bookshelf)

### NUMA as a Library
Think of a NUMA machine as a system of massive, distinct **libraries**.
```text
Virtual/physical memory → Pages → NUMA nodes
```
A large chunk of data (a book section or memory page) is placed in a specific library. You go to the nearest library to access it. Crucially, a NUMA node contains the actual, physical DRAM where that memory lives.

### TILEPro64 as a Distributed Bookshelf
Think of the TILEPro64 as a highly **distributed bookshelf system** across an office.
```text
Memory addresses → Cache lines → Home Caches
```
Small pieces of data (individual books or cache lines) are assigned to different distributed tile caches. The network routes requests to the specific tile responsible for managing that line. 

Importantly, the [[Home Cache]] is not necessarily the permanent physical memory (DRAM). It primarily participates in **cache coherence** and tracking, meaning other tiles may also hold copies of the data, but the home cache acts as the manager.

## Translating the Scheduling Algorithm
Because the core philosophy—*Put data intelligently → Record location → Calculate dealing cost → Execute near data*—is highly portable, the scheduling algorithms map almost 1:1 between architectures:

| Concept | NUMA System | TILEPro64 Manycore |
| :--- | :--- | :--- |
| **Data Unit** | Memory Pages | Cache Lines |
| **Physical Location** | NUMA Node | [[Home Cache]] |
| **Distance Metric** | Distance from NUMA Node | Access Latency to Home Cache |
| **Cost Matrix Source** | OS provides NUMA distances | Runtime benchmarks latency at startup |
| **Task Queue** | Bound to NUMA Node | Bound to Home Cache |
| **Fast-Path Cutoff** | Last-Level Cache (LLC) | Private L1 Cache |
| **Load Balancing** | Global [[Work Stealing]] | Vicinity-limited [[Work Stealing]] |

## See Also
- [[TILEPro64 Architecture]]
- [[UMA vs NUMA]]
- [[Home Cache]]
- [[Runtime-Assisted Data Distribution]]
