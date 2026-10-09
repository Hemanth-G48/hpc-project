# Portability of Runtime Data Distribution

A primary contribution of modern [[Locality-Aware Task Scheduling|locality-aware runtimes]] is providing an abstraction layer that hides the underlying hardware complexity from the programmer, ensuring that data distribution strategies remain portable across vastly different architectures.

## The General Non-Uniform Access Problem

Locality issues are not strictly limited to traditional [[UMA vs NUMA|NUMA]] systems (where memory is physically divided across distinct DRAM nodes). They also appear in manycore processors like the [[TILEPro64 Architecture|TILEPro64]], which exhibit "on-chip NUMA-like effects" due to banked shared caches. 

Although the physical hardware mechanisms are completely different, the core locality problem is analogous:
```text
              Non-uniform access
                     │
          ┌──────────┴──────────┐
          ↓                     ↓
       NUMA                  TILEPro64
          │                     │
    Memory pages           Cache lines
          │                     │
    NUMA nodes              Home caches
          │                     │
          └──────────┬──────────┘
                     ↓
              Locality matters
                     ↓
       Data placement + scheduling
```

## Abstracting Hardware Details

Instead of forcing a programmer to write machine-specific code (e.g., "distribute memory pages across NUMA nodes" vs. "distribute cache lines across Home Caches"), a portable runtime relies on high-level [[Data Distribution Policies]] (such as Standard, Fine, or Coarse).

The runtime dynamically translates these generic hints into the appropriate architecture-specific operations:

| System | Distribution Unit | Target Location |
| :--- | :--- | :--- |
| **NUMA** | Memory Page (Large-grain) | NUMA Node |
| **TILEPro64** | Cache Line (Fine-grain) | [[Home Cache]] |

By demonstrating that the same locality-aware scheduling and distribution philosophy yields performance improvements on both a conventional NUMA system and a manycore architecture, researchers prove that their framework is a **generalizable solution** for non-uniform access costs, rather than just a NUMA-specific optimization.

## See Also
- [[Runtime-Assisted Data Distribution]]
- [[NUMA vs TILEPro64 Architecture]]
- [[Data Distribution Policies]]
