# Data Distribution Policies

When using [[Runtime-Assisted Data Distribution]], the programmer can specify high-level policies that govern how the runtime system places data across multiple memory locations (e.g., NUMA nodes or cache banks).

Three simple, foundational policies are typically provided:

| Policy | Meaning |
| :--- | :--- |
| **Standard** | Let the operating system decide where data goes (no explicit distribution control). |
| **Fine** | Distribute individual data units round-robin across locations. |
| **Coarse** | Distribute each allocation as a unit using round-robin across locations. |

## Example Scenario

Assume there are 4 memory locations (`N0`, `N1`, `N2`, `N3`) and the data to allocate consists of units `D0` through `D7`.

### Standard Distribution
The OS handles placement opaquely. The programmer does not control or predict where `D0` or `D1` end up.

### Fine Distribution (Unit-wise Round-Robin)
Each individual data unit is distributed sequentially across the available locations:
* `D0 → N0`
* `D1 → N1`
* `D2 → N2`
* `D3 → N3`
* `D4 → N0`
* `D5 → N1`
* `D6 → N2`
* `D7 → N3`

Resulting placement:
```text
N0: D0, D4
N1: D1, D5
N2: D2, D6
N3: D3, D7
```
> *Performance Portability Note:* Experimental results show that utilizing this runtime `fine` policy is a feasible, portable replacement for writing OS-specific `numactl` page interleaving logic. Both achieve equivalent parallel bandwidth spreading.

### Coarse Distribution
Instead of distributing fine-grained units, entire logical allocations are distributed. 
* `Allocation A → N0`
* `Allocation B → N1`
* `Allocation C → N2`
* `Allocation D → N3`
* `Allocation E → N0`

The unit of distribution here is the full allocation block rather than individual data elements.

## Global Policy vs. Override

Programmers can apply these policies globally without altering source code by using environment variables:
```bash
export OMP_DATA_DISTRIBUTION=fine
./program
```
This applies the `fine` policy to all `omp_malloc` calls. However, for a gradual optimization approach, individual `omp_malloc` calls can provide specific hints that **override** the global policy (e.g., leaving the global policy as `fine` but specifying `coarse` for one specific problematic array).

## The Heuristic: Choosing Fine vs Coarse

To decide between Fine and Coarse distribution, consider the number of `malloc` calls and how many tasks operate on the allocated data:

| | **One task** | **Many tasks** |
| :--- | :--- | :--- |
| **One malloc** | Regular malloc | **Fine** |
| **Many mallocs** | **Coarse** | **Coarse** |

### The Intuition

The core difference is whether the runtime is trying to **spread data** to maximize bandwidth, or **keep an allocation together** to maximize locality.

| | **Fine** | **Coarse** |
| :--- | :--- | :--- |
| **What is distributed?** | Individual pages/units | Whole allocations |
| **One allocation** | Spread across nodes | Associated with one node |
| **Locality** | Lower / less predictable | Stronger |
| **Parallel bandwidth** | Excellent | Good when tasks use *different* allocations |
| **Best for** | Many threads sharing one allocation | Tasks working independently on separate allocations |

#### The Warehouse Analogy
Imagine 4 warehouses and shipments of boxes:

**Fine (Spread pieces):**
One single shipment `A` arrives. `Box 1 → Warehouse 0`, `Box 2 → Warehouse 1`, `Box 3 → Warehouse 2`, etc. The *same shipment is spread everywhere*, allowing multiple workers across different warehouses to process parts of it simultaneously.

**Coarse (Keep together):**
Four separate shipments arrive. `Shipment A → Warehouse 0`, `Shipment B → Warehouse 1`, `Shipment C → Warehouse 2`. Each shipment stays contiguous. A worker assigned to Shipment A can just stay in Warehouse 0. 

> *Note:* "Coarse" in the abstraction means allocations are assigned round-robin to locations, so the locality of *each* allocation is preserved compared to Fine's unit-by-unit scattering.

## Extensibility (Plug-ins)
While Standard, Fine, and Coarse are the foundational policies, the abstraction is designed to be extensible. Runtime developers can add **custom policies as plug-ins** without changing the underlying programming interface. Future policies could include:
* `NUMA-aware`
* `Topology-aware`
* `Bandwidth-aware`
* `Latency-aware`

## See Also
- [[Runtime-Assisted Data Distribution]]
- [[UMA vs NUMA]]
