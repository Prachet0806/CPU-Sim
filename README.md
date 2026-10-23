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
* **Configurable issue width** (1-8 instructions per cycle)
* **Blocking L1 cache** with set-associative organization and LRU replacement; only one access can be outstanding, and its port stays occupied through the completion cycle
* **Branch misprediction cost** added as an abstract cycle penalty to a fixed instruction stream
* **Store-to-load address dependency** stalls
* **Structural hazard** on single memory port

The simulator is designed to study **performance trends and sensitivities**, not to produce cycle-accurate results.

Each pipeline stage takes at least one cycle. A cache access started in cycle `t`
with configured latency `L` completes at the end of cycle `t + L - 1` and can
retire from Writeback in the next cycle. The next memory access can start in
cycle `t + L`. Younger instructions cannot retire ahead of an older stalled
load. Independent younger work may reach the Memory stage, but it cannot pass
the stalled load.

The `raw_hazard_stalls` and `structural_hazard_stalls` counters count blocked
**instruction-cycle events**, not elapsed cycles. Each blocked instruction can
add one event in a cycle, so several blocked instructions can add several events
in the same cycle. These counters are not additive components of `total_cycles`.

---

## ⚡ Key Features

### 1. Pipeline Modeling

* Models a **5-stage in-order pipeline** (Fetch → Decode → Execute → Memory → Writeback)
* Captures pipeline stalls caused by:
  * **Structural hazards** (busy pipeline stages, single memory port)
  * **Data hazards** (RAW dependencies on memory addresses)
* Adds a configurable branch-misprediction penalty to the reported cycle total
* **Fixed five-stage pipeline** and configurable issue width (1-8)
* Multiple instructions can advance per cycle where stage and port capacity permits

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

* Implements a **probabilistic branch predictor** with configurable accuracy
* Models prediction accuracy (e.g., 95%) using a fixed-seed RNG for deterministic replay
* Measures:
  * Misprediction frequency
  * Added penalty cycles
* Branch prediction occurs when a branch enters **Fetch**. The penalty changes
  the reported cycle total, but does not stall or flush the pipeline, discard
  younger instructions, or redirect execution. Synthetic workloads have no
  alternate branch path, so mispredictions preserve the generated stream.
* **Note:** The bundled MatMul and Conv1D workloads do not emit branch instructions;
  use `--workload branch` to exercise prediction and penalty accounting.

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
  * Avoids materializing the entire stream; workload parameters and address ranges remain finite
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
├── src/test_main.cpp  # Self-contained architecture regression tests
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
./build/Release/cpu-sim.exe --config tests/conv1d_config.json
```

### Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `--cache-size <bytes>` | L1 cache size (power of 2) | 32768 |
| `--cache-line-size <bytes>` | Cache line size | 64 |
| `--cache-assoc <ways>` | Associativity (power of 2) | 8 |
| `--cache-hit-latency <cycles>` | Hit latency | 4 |
| `--cache-miss-latency <cycles>` | Miss latency | 100 |
| `--pipeline-stages <n>` | Pipeline stages (fixed at 5) | 5 |
| `--issue-width <n>` | Issue width (1-8) | 1 |
| `--bp-accuracy <0-1>` | Branch predictor accuracy | 0.95 |
| `--bp-penalty <cycles>` | Misprediction penalty | 15 |
| `--workload <matmul\|conv1d\|branch>` | Workload type | matmul |
| `--matmul-n <n>` | Matrix dimension (1-8192) | 64 |
| `--conv1d-len <n>` | Input length (1-67108864) | 1000 |
| `--conv1d-kernel <n>` | Kernel size (1 to input length) | 3 |
| `--branch-count <n>` | Branches in branch workload | 1000 |
| `--json` | JSON output | false |
| `--progress` | Show progress | false |
| `--progress-interval <n>` | Progress report interval | 1000000 |
| `--config <file>` | Load JSON config file | - |

### JSON configuration

The config file is a flat JSON object. It accepts CLI-style names such as
`cache_size` and `issue_width`, and result-style aliases such as
`l1_cache_size` and `pipeline_issue_width`. Use `workload` or `workload_type`
with `matmul`, `conv1d`, or `branch`. Invalid, duplicate, or unknown fields
cause an error. See `tests/conv1d_config.json` for an example.

The `--json` result also contains measured fields such as `total_cycles` and
`cache_misses`; it is a result document, not a configuration file.

---

## 🧪 Validation Strategy

Validation is performed in two complementary layers:

### 1. Unit Verification (Contract Testing)

A self-contained test runner validates architectural invariants:

* Re-accessing the same address produces a cache hit
* Direct-mapped caches exhibit conflict misses
* Branch misprediction penalties preserve instruction counts
* Accepted instructions retire in order, even with a memory miss
* Store-to-load hazards and the single memory port are enforced
* Two independent misses cannot overlap, including the first miss's completion cycle
* Hit and miss latency boundaries, sustained issue-width throughput, repeated runs, and branch penalties are checked
* Invalid configuration values fail in Release builds

`Simulator::run()` requires an empty pipeline, returns fresh statistics for
each invocation, and drains the pipeline before returning. A reused cache
stays warm, and a reused branch predictor continues from its current random
state. Reset a workload explicitly to replay it. `Pipeline::flush()` manually
discards in-flight instructions and clears an outstanding memory access. It
does not reset lifetime stall-event counters or the cache and predictor. Branch
mispredictions never call `flush()`.

Run with: `cmake -B build -DBUILD_TESTS=ON && cmake --build build --config Release && ctest --test-dir build --output-on-failure -C Release`

### 2. Behavioral Validation (Sensitivity Analysis)

The simulator is validated by sweeping L1 cache sizes and observing miss-rate trends for a fixed workload.

For a **64×64 Matrix Multiplication** (~49KB working set):

* The sweep records the miss count for each cache size
* Trends can reveal capacity and mapping effects, but do not by themselves
  establish cycle accuracy or agreement with a physical CPU

Validation is automated using:

```bash
python3 tools/validate_sensitivity.py --exe build/Release/cpu-sim.exe
```

On Linux or macOS, use `--exe build/cpu-sim`. Plotting requires Matplotlib
and writes `sensitivity_curve.png` by default; use `--output` to choose a path.
Use `--no-plot` to run the numeric sweep without Matplotlib.
Install the plotting dependency with `python3 -m pip install -r tools/requirements.txt`.

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
* [x] Add a branch-emitting workload to exercise branch prediction
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
* Self-contained regression tests and CI builds

It is intended as a **performance exploration and validation tool**, not a functional CPU emulator.
