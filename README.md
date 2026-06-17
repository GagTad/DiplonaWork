### Live Telemetry Dashboard
Real-time graphs plotting **HPWL Reduction**, **Temperature Decay**, and **Acceptance Rate** metrics simultaneously using QtCharts.
---

## Algorithmic Deep Dive

### 1. Wirelength Optimization: Incremental HPWL
Traditional placement tools calculate the Half-Perimeter Wirelength (HPWL) of the entire netlist on every single cell movement, scaling at $O(N^2)$ or $O(M \times N)$. 
This engine implements **Incremental HPWL**. When a cell moves:
1. Only the nets directly connected to that specific cell are marked as dirty.
2. The delta change ($\Delta \text{HPWL}$) is computed in $O(K)$ time, where $K$ is the net pin count.
3. The total wirelength is updated instantly via accumulator variables.

$$\Delta \text{HPWL} = \sum_{n \in \text{Nets}(\text{cell})} \left( \text{HPWL}_{\text{new}}(n) - \text{HPWL}_{\text{old}}(n) \right)$$

### 2. Collision Detection: 2D Spatial Grid
To check if a cell move creates illegal overlaps during the global placement phase, a **2D Spatial Grid** divides the floorplan into uniform geometric bins. Cells are hashed into these bins based on their $(x, y)$ coordinates. Overlap checks are bound to neighboring bins only, translating an $O(N)$ scanning cost into an amortized $O(1)$ lookup.

### 3. Post-SA Greedy Legalizer
Simulated Annealing functions continuously, often leaving minor fractional cell overlaps or off-grid misalignments. The **Greedy Legalizer** handles legalization by:
* Sorting cells by their optimized X-coordinates.
* Packing cells sequentially into defined circuit rows.
* Aligning cell boundaries to the site grid while minimizing displacement from the SA-calculated optimal positions.

Developed with ❤️ by Gagik.
