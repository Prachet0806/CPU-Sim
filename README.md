# CPU Microarchitecture Simulator & AI Workload Profiler

A pre-silicon, behavioral CPU performance simulator designed to analyze the interaction between **software algorithms** (specifically AI-style kernels) and **hardware microarchitecture** (Cache, Pipeline, Branch Prediction).

**Status:** Active  
**Language:** C++17  
**Focus:** Computer Architecture · Systems Performance · Pre-Silicon Validation

---

## 📖 Project Overview

This project is not an instruction set emulator (like QEMU) or a cycle-accurate RTL model.  
Instead, it is a **behavioral simulator** that models the performance characteristics of a modern CPU.

The goal is to answer architectural trade-off questions, such as:

* *How does L1 cache size impact the execution time of a matrix multiplication kernel?*
* *What is the performance cost of branch misprediction in a deep pipeline?*
* *How do memory access patterns in convolution layers drive cache eviction?*
* *How does superscalar issue width affect throughput for memory-bound vs compute-bound workloads?*
* *What is the impact of RAW data hazards on pipeline utilization?*

This tool simulates the **hardware cost of software**, focusing on latency, stalls, and memory hierarchy effects rather than register-level correctness.

### 🎯 Intended Audience

* Systems / Platform Engineers
* Pre-Silicon Software & Architecture Engineers
* Performance Analysis & Tooling Engineers
* Computer Architecture Students & Researchers

---

## ⚙ Execution Model (Explicit Assumptions)

The simulator models a simplified CPU with the following assumptions:

* **In-order execution** with 5-stage pipeline (Fetch, Decode, Execute, Memory, Writeback)
* **Configurable superscalar issue width** (1-N instructions per cycle)
* **Blocking L1 cache** with set-associative organization and LRU replacement
* **Full pipeline flush on branch misprediction** (penalty includes refetch cycles)
* **RAW data hazard detection** on memory addresses (LOAD/STORE dependencies)
* **Structural hazard** on single memory port

The simulator is designed to study **performance trends and sensitivities**, not to produce cycle-accurate results.

---

## ⚡ Key Features

### 1. Pipeline Modeling

* Models a **5-stage in-order pipeline** (Fetch → Decode → Execute → Memory → Writeback)
* Captures pipeline stalls caused by:
  * **Structural hazards** (busy pipeline stages, single memory port)
  * **Control hazards** (branch mispredictions with configurable penalty)
  * **Data hazards** (RAW dependencies on memory addresses)
* **Configurable pipeline depth** and **issue width** (superscalar support)
* **True superscalar modeling**: multiple instructions per cycle through pipeline stages

### 2. Cache Hierarchy Modeling

* **Set-associative L1 cache** with configurable parameters
* Configurable: size, line size, associativity, hit/miss latency
* Implements:
  * **Bitwise address decoding** (tag / index / offset)
  * **LRU replacement policy** with global timestamp
  * **Deterministic behavior** for reproducible experiments
* Enables sensitivity analysis to visualize:
  * Temporal locality
  * Spatial locality
  * Capacity-driven cache thrashing ("cache cliff" behavior)
  * Conflict misses in direct-mapped configurations

### 3. Branch Prediction

* Implements a **probabilistic branch predictor**
* Models prediction accuracy (e.g., 95%)
* Measures:
  * Misprediction frequency
  * Pipeline flush penalties
* Branch prediction at **Fetch stage** (realistic timing)
* Uses a fixed RNG seed for deterministic replay

### 4. AI-Representative Workloads

The simulator includes synthetic workloads that represent common AI memory behaviors:

* **GEMM / Matrix Multiplication**
  * O(N³) access pattern with N×N matrices
  * Stresses cache capacity and conflict behavior
  * Highlights row-major vs column-major locality issues (column-strided B matrix access)
  * Streaming instruction generation (O(1) memory)

* **Conv1D (1D Convolution)**
  * Sliding window access pattern
  * High temporal reuse of kernel weights
  * Models inference-style streaming behavior
  * Streaming instruction generation (O(1) memory)

* **Streaming Workload Interface**
  * On-demand instruction generation via `has_next()`/`next()`
  * Enables simulation of arbitrarily large problem sizes
  * Legacy `instructions()` method retained for backward compatibility

---

## 🏗 Code Architecture

The codebase enforces a strict separation between **architecture models** (hardware) and **workload drivers** (software), mirroring real pre-silicon development workflows.

```text
cpu-sim/
├── include/
│   ├── arch/        # Hardware contracts (Pipeline, Cache, Branch Predictor)
│   ├── sim/         # Simulator interface
│   └── workload/    # Workload interfaces
├── src/
│   ├── arch/        # Microarchitecture implementations
│   ├── sim/         # Main simulation loop
│   └── workload/    # AI-style workload generators
├── tools/
│   └── validate_sensitivity.py  # Cache-size sweep & visualization
├── tests/           # Unit tests (Catch2)
├── .github/workflows/  # CI configuration
├── CMakeLists.txt   # Build configuration
└── README.md
```

---

## 🚀 Quick Start

### Build (CMake)

```bash
# Configure
cmake -B build -DBUILD_TESTS=ON

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build --output-on-failure -C Release
```

### Run a Simulation

```bash
# Run Matrix Multiplication with a 32KB L1 cache (default workload)
./build/Release/cpu-sim.exe --cache-size 32768

# Run with JSON output for scripting
./build/Release/cpu-sim.exe --cache-size 32768 --json

# Run Conv1D workload
./build/Release/cpu-sim.exe --workload conv1d --conv1d-len 1000 --conv1d-kernel 3

# Run with superscalar (2-issue) and progress reporting
./build/Release/cpu-sim.exe --cache-size 32768 --issue-width 2 --progress

# Load configuration from JSON file
./build/Release/cpu-sim.exe --config my_config.json
```

### Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `--cache-size <bytes>` | L1 cache size (power of 2) | 32768 |
| `--cache-line-size <bytes>` | Cache line size | 64 |
| `--cache-assoc <ways>` | Associativity (power of 2) | 8 |
| `--cache-hit-latency <cycles>` | Hit latency | 4 |
| `--cache-miss-latency <cycles>` | Miss latency | 100 |
| `--pipeline-stages <n>` | Pipeline stages | 5 |
| `--issue-width <n>` | Superscalar issue width | 1 |
| `--bp-accuracy <0-1>` | Branch predictor accuracy | 0.95 |
| `--bp-penalty <cycles>` | Misprediction penalty | 15 |
| `--workload <matmul\|conv1d>` | Workload type | matmul |
| `--matmul-n <n>` | Matrix dimension | 64 |
| `--conv1d-len <n>` | Input length | 1000 |
| `--conv1d-kernel <n>` | Kernel size | 3 |
| `--json` | JSON output | false |
| `--progress` | Show progress | false |
| `--progress-interval <n>` | Progress report interval | 1000000 |
| `--config <file>` | Load JSON config file | - |

### Sample Output (Human-Readable)

```text
[SIMULATION COMPLETE]
Workload:             Matrix Multiplication
Matrix Size (N):      64
L1 Cache Size:        32768 Bytes
L1 Line Size:         64 Bytes
L1 Associativity:     8-way
Pipeline Stages:      5
Issue Width:          1
BP Accuracy:          0.95
BP Mispredict Penalty:15 cycles
--------------------------------
Instructions Retired: 794624
Total Cycles:         2998276
IPC:                  0.27
--------------------------------
Cache Accesses:       532480
Cache Hits:           531712
Cache Misses:         768
Branch Predictions:   0
Branch Mispredicts:   0
--------------------------------
```

### Sample Output (JSON)

```json
{
  "workload": "matmul",
  "l1_cache_size": 32768,
  "l1_line_size": 64,
  "l1_associativity": 8,
  "l1_hit_latency": 4,
  "l1_miss_latency": 100,
  "pipeline_stages": 5,
  "pipeline_issue_width": 1,
  "bp_accuracy": 0.95,
  "bp_mispredict_penalty": 15,
  "instructions_retired": 794624,
  "total_cycles": 2998276,
  "ipc": 0.27,
  "cache_misses": 768,
  "cache_hits": 531712,
  "cache_accesses": 532480,
  "branch_mispredicts": 0,
  "branch_predictions": 0
}
```

---

## 🧪 Validation Strategy

Validation is performed in two complementary layers:

### 1. Unit Verification (Contract Testing)

A dedicated test runner (Catch2) validates architectural invariants:

* Re-accessing the same address produces a cache hit
* Direct-mapped caches exhibit conflict misses
* Branch misprediction penalties are honored
* Pipeline correctly models RAW hazards and structural hazards

Run with: `ctest --test-dir build -C Release`

### 2. Behavioral Validation (Sensitivity Analysis)

The simulator is validated by sweeping L1 cache sizes and observing miss-rate trends for a fixed workload.

For a **64×64 Matrix Multiplication** (~49KB working set):

* Severe thrashing occurs when cache < working set
* Miss rate drops sharply once cache capacity exceeds the working set
* The resulting **"cache cliff"** confirms correct architectural behavior

Validation is automated using:

```bash
python3 tools/validate_sensitivity.py
```

![Sensitivity Curve](sensitivity_curve.png)

The output is a plot of cache misses vs cache size, demonstrating expected physical behavior.

*Validation focuses on qualitative correctness (trends and cliffs) rather than absolute cycle accuracy.*

---

## 🧠 Design Rationale: Why This Matters

### The AI–Architecture Gap

Modern AI workloads are often **memory-bound**, not compute-bound.  
This simulator demonstrates why large tensor operations can overwhelm general-purpose CPU caches, motivating specialized hardware such as NPUs and AI accelerators.

### Pre-Silicon Mindset

In real hardware development:

* Chips are expensive and slow to fabricate
* Architectural decisions must be validated *before* silicon exists

This project mirrors the **pre-silicon workflow**, where software models are used to evaluate trade-offs such as cache sizing, pipeline depth, branch behavior, and issue width long before hardware is built.

---

## 🔮 Future Improvements

* [ ] Add multi-level cache hierarchy (L2 / L3)
* [x] Support superscalar execution (issue width > 1) - **Implemented**
* [ ] Implement out-of-order execution (scoreboarding / Tomasulo)
* [ ] Add CPI breakdown reporting (stall cycle categorization)
* [ ] Add I-cache modeling separate from D-cache
* [ ] Support for trace-based workloads (external trace files)

---

## 📌 Summary

This project demonstrates:

* Systems-level C++ design (RAII, modern C++17, clean architecture)
* Hardware–software co-design thinking
* Pre-silicon performance modeling
* AI workload–driven architectural analysis
* Streaming workload simulation for scalability
* Comprehensive testing infrastructure (Catch2, CI/CD)

It is intended as a **performance exploration and validation tool**, not a functional CPU emulator.