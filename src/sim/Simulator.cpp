#include "sim/Simulator.h"

namespace sim {

Simulator::Simulator(arch::Pipeline& p, arch::Cache& c, arch::BranchPredictor& bp)
    : pipeline_(p), cache_(c), predictor_(bp) {}

SimStats Simulator::run(const workload::Workload& workload) {
    SimStats stats{0, 0, 0, 0};

    for (const auto& instr : workload.instructions()) {
        uint32_t latency = pipeline_.execute(instr);

        if (instr.type == arch::InstrType::LOAD || instr.type == arch::InstrType::STORE) {
            uint32_t mem = cache_.access(instr.memory_address);
            if (mem == cache_.miss_latency()) ++stats.cache_misses;
            latency += mem;
        }

        if (instr.type == arch::InstrType::BRANCH) {
            uint32_t penalty = predictor_.predict();
            if (penalty > 0) {
                ++stats.branch_mispredicts;
                latency += penalty;
            }
        }
        stats.cycles += latency;
        ++stats.instructions;
    }
    return stats;
}

} // namespace sim