# Locality-Aware Task Scheduling & Data Distribution (Paper Index)

This index provides a structured Map of Content (MOC) for the research paper exploring the synergy between runtime-assisted data distribution and locality-aware task scheduling on NUMA and manycore architectures.

## 1. The Core Problem
* [[Data Locality in Task Scheduling]]
* [[UMA vs NUMA]]
* [[Memory Bandwidth vs Latency in NUMA]]
* [[Work Stealing]]
* [[OpenMP Data Placement Limitations]]

## 2. Hardware Architectures
* [[NUMA vs TILEPro64 Architecture]] (The Library vs Bookshelf analogy)
* [[TILEPro64 Architecture]]
* [[Home Cache]]
* [[NUMA Distance]]

## 3. Data Distribution (Placing the Data)
* [[Runtime-Assisted Data Distribution]]
* [[Data Distribution Policies]] (Fine vs Coarse, Global vs Override)
* [[Data Distribution on TILEPro64]]
* [[omp_malloc]]
* [[Portability of Runtime Data Distribution]]
* [[First-Touch Memory Allocation]]

## 4. Task Scheduling (Placing the Task)
* [[Locality-Aware Task Scheduling]]
* [[Work-Dealing]] (Choosing the initial queue)
* [[Task Data Footprint]] (Using OpenMP `depend` clauses)
* [[Dispatch Stall Cycles]]

## 5. Experimental Results & Synthesis
* [[Scheduling vs Data Distribution Synergy]] (Map vs Matmul vs Vecmul vs Reduction)

## 6. Related Work & Positioning
* [[Related Work]] (Manual vs automatic placement, locality-aware scheduling, and where this paper sits)
* [[Related Work - Design Inspirations]] (Where each design piece came from: Memphis, HPCToolkit, Schmidl, and the manycore works — Yoo, Vikranth, R-NUCA)

---
> **Central Thesis:** There is no universally best data distribution or scheduling strategy. The best performance comes from matching data distribution, task scheduling, and the application's access pattern. A locality-aware scheduler can serve as a safe default because it adapts: exploiting locality when it exists, and gracefully falling back to load balancing when it does not.
