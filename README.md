### 2. Live Telemetry Dashboard
Real-time graphs plotting **HPWL Reduction**, **Temperature Decay**, and **Acceptance Rate** metrics simultaneously using QtCharts.
---

## 🧠 Algorithmic Deep Dive

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

---

## 📊 Performance Benchmarks

Below is an evaluation contrasting the naive global placement approach against the optimized architecture across various standard cell benchmarks:

| Benchmark Netlist | Cell Count | Net Count | Naive Runtime $O(N^2)$ | Optimized Runtime $O(K)$ | Speedup Factor | Final Overlap % |
|:-------------------|:------------|:-----------|:-----------------------|:--------------------------|:----------------|:-----------------|
| **ckt_small_100** | 100         | 120        | 1.42 sec               | 0.002 sec                 | ~710x           | 0.00% (Legal)    |
| **ckt_med_500** | 500         | 650        | 42.15 sec              | 0.031 sec                 | ~1360x          | 0.00% (Legal)    |
| **ckt_large_2000** | 2,000       | 2,400      | 14.8 mins              | 0.285 sec                 | ~3100x          | 0.00% (Legal)    |

---

## 🛠️ Project Structure

```text
vlsi-placement-tool/
├── src/
│   ├── core/                  # Pure C++ EDA Engine Core
│   │   ├── Cell.h / Cell.cpp  # Physical cell data representations
│   │   ├── Net.h / Net.cpp    # Routing and pin connection logic
│   │   ├── PlacementEngine.h  # Simulated Annealing & Legalizer core
│   │   └── SpatialGrid.h      # Custom 2D binning implementation
│   ├── gui/                   # Qt6 User Interface Layouts
│   │   ├── MainWindow.h/.cpp  # Master UI controller
│   │   ├── PlacementView.h    # Custom 2D canvas drawing the silicon dies
│   │   └── TelemetryChart.h   # QtCharts mapping performance graphs
│   └── main.cpp               # Multi-threaded orchestration entry point
├── docs/                      # Thesis reports and slide assets
├── benchmarks/                # Custom input test files (.txt/.v)
└── CMakeLists.txt             # Project build specifications

🔨 How to Build & Run
Prerequisites
C++17 Compliant Compiler (GCC 9+, Clang 10+, or MSVC 2019+)

CMake (v3.16+)

Qt 6 SDK (Components required: Qt6::Core, Qt6::Widgets, Qt6::Charts)

Building from Source
Clone the repository:


......

🤝 Acknowledgments
Special thanks to the instructors and academic advisors at Synopsys Armenia and university faculty for their technical guidance in Electronic Design Automation concepts.

Algorithmic concepts inspired by foundational industry literature on Simulated Annealing Optimization (Kirkpatrick et al.) and TimberWolf placement systems.

Developed with ❤️ by Gagik.
