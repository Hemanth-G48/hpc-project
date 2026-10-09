# TILEPro64 Architecture

The **TILEPro64** is a 64-core manycore processor organized as a grid of tiles. 

## Structure
Each tile typically contains:
* A processing core
* Local L1 and L2 caches

Conceptually, the grid looks like this:
```text
┌──────┬──────┬──────┬──────┐
│Tile  │Tile  │Tile  │Tile  │
│Core  │Core  │Core  │Core  │
│L1/L2 │L1/L2 │L1/L2 │L1/L2 │
├──────┼──────┼──────┼──────┤
│ ...  │ ...  │ ...  │ ...  │
└──────┴──────┴──────┴──────┘
```

## The On-Chip NUMA Effect
While traditional [[UMA vs NUMA|NUMA]] systems experience variable memory latency when accessing distinct physical DRAM modules, the TILEPro64 experiences an **on-chip NUMA-like effect** due to its cache coherence architecture. 

Specifically, every cache line in the system is assigned to a specific [[Home Cache]]. The physical distance on the silicon grid between a requesting core and the data's Home Cache dictates the access latency. Accessing a remote home cache can be 4–6× slower than accessing a local one.

Because of this topology, [[Locality-Aware Task Scheduling]] and intelligent [[Data Distribution on TILEPro64|Data Distribution]] are critical for high performance, just as they are in multi-node NUMA systems (see [[NUMA vs TILEPro64 Architecture]]).

## See Also
- [[Home Cache]]
- [[NUMA vs TILEPro64 Architecture]]
- [[Data Distribution on TILEPro64]]
