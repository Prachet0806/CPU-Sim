#pragma once
#include <cstdint>
#include "arch/Pipeline.h"
#include "arch/Cache.h"
#include "arch/BranchPredictor.h"
#include "workload/Workload.h"

namespace sim {

struct SimStats {
    uint64_t cycles;
    uint64_t instructions;
    uint64_t cache_misses;
    uint64_t branch_mispredicts;
};

class Simulator {
public:
    Simulator(arch::Pipeline& p, arch::Cache& c, arch::BranchPredictor& bp);
    SimStats run(const workload::Workload& workload);

private:
    arch::Pipeline&        pipeline_;
    arch::Cache&           cache_;
    arch::BranchPredictor& predictor_;
};

} // namespace sim