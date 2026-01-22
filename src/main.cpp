#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>

#include "arch/Pipeline.h"
#include "arch/Cache.h"
#include "arch/BranchPredictor.h"
#include "sim/Simulator.h"
#include "workload/MatMulWorkload.h"

int main(int argc, char* argv[])
{
    // 1. Configure Hardware
    // Default 32KB, or use command line arg
    uint32_t l1_size = 32 * 1024;
    if (argc > 1) {
        l1_size = std::atoi(argv[1]);
    }

    arch::PipelineConfig pipe_cfg{5, 1};
    arch::CacheConfig    cache_cfg{l1_size, 64, 8, 4, 100};
    arch::BranchPredictorConfig bp_cfg{0.95f, 15};

    arch::Pipeline pipeline(pipe_cfg);
    arch::Cache cache(cache_cfg);
    arch::BranchPredictor bp(bp_cfg);

    // 2. Select Workload
    // N=64 -> ~49KB working set
    workload::MatMulWorkload workload(64);

    // 3. Run Simulation
    sim::Simulator simulator(pipeline, cache, bp);
    auto stats = simulator.run(workload);

    // 4. Report Results (Professional Table)
    double ipc = static_cast<double>(stats.instructions) / stats.cycles;

    std::cout << "\n[SIMULATION COMPLETE]\n";
    std::cout << "Workload:             Matrix Multiplication (N=64)\n";
    std::cout << "L1 Cache Size:        " << l1_size << " Bytes\n";
    std::cout << "--------------------------------\n";
    std::cout << "Instructions Retired: " << stats.instructions << "\n";
    std::cout << "Total Cycles:         " << stats.cycles << "\n";
    std::cout << "IPC:                  " << std::fixed << std::setprecision(2) << ipc << "\n";
    std::cout << "--------------------------------\n";
    std::cout << "Cache Misses:         " << stats.cache_misses << "\n";
    std::cout << "Branch Mispredicts:   " << stats.branch_mispredicts << "\n";
    std::cout << "--------------------------------\n";

    return 0;
}