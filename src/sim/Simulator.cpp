//src/sim/Simulator.cpp
#include "sim/Simulator.h"
#include "arch/Pipeline.h"
#include "arch/Cache.h"
#include "arch/BranchPredictor.h"
#include <cstdint>
#include <vector>

namespace sim {

// Cache adapter implementing Pipeline's CacheInterface
class CacheAdapter : public arch::CacheInterface {
public:
    explicit CacheAdapter(arch::Cache& cache, SimStats* stats) : cache_(cache), stats_(stats) {}
    
    uint32_t access(uint64_t address) override {
        uint32_t latency = cache_.access(address);
        if (stats_) {
            ++stats_->cache_accesses;
            if (latency == cache_.miss_latency()) {
                ++stats_->cache_misses;
            } else {
                ++stats_->cache_hits;
            }
        }
        return latency;
    }
    
    uint32_t hit_latency() const override { return cache_.hit_latency(); }
    uint32_t miss_latency() const override { return cache_.miss_latency(); }
    
private:
    arch::Cache& cache_;
    SimStats* stats_;
};

// Branch predictor adapter implementing Pipeline's BranchPredictorInterface
class BranchPredictorAdapter : public arch::BranchPredictorInterface {
public:
    explicit BranchPredictorAdapter(arch::BranchPredictor& bp, SimStats* stats) : bp_(bp), stats_(stats) {}
    
    uint32_t predict(uint64_t pc) override {
        (void)pc;
        if (stats_) ++stats_->branch_predictions;
        uint32_t penalty = bp_.predict();
        if (penalty > 0 && stats_) ++stats_->branch_mispredicts;
        return penalty;
    }
    
private:
    arch::BranchPredictor& bp_;
    SimStats* stats_;
};

Simulator::Simulator(arch::Pipeline& p, arch::Cache& c, arch::BranchPredictor& bp)
    : pipeline_(p), cache_(c), predictor_(bp) {}

void Simulator::set_progress_callback(ProgressCallback cb, uint64_t interval) {
    progress_callback_ = cb;
    progress_interval_ = interval;
    next_progress_ = interval;
}

SimStats Simulator::run(workload::Workload& workload) {
    return run(workload, UINT64_MAX);
}

SimStats Simulator::run(workload::Workload& workload, uint64_t max_instructions) {
    SimStats stats{};
    
    // Create adapters
    CacheAdapter cache_adapter(cache_, &stats);
    BranchPredictorAdapter bp_adapter(predictor_, &stats);
    
    // Set adapters in pipeline
    pipeline_.set_cache_interface(&cache_adapter);
    pipeline_.set_branch_predictor_interface(&bp_adapter);
    
    // Program counter for branch prediction
    uint64_t pc = 0;
    const uint64_t pc_increment = 4;  // Assume 4-byte instructions
    
    // Streaming execution
    while (workload.has_next() && stats.instructions < max_instructions) {
        // Check if pipeline can accept more instructions
        if (pipeline_.can_accept()) {
            // Fetch up to issue_width instructions
            std::vector<arch::Instruction> instrs;
            std::vector<uint64_t> pcs;
            
            for (uint32_t i = 0; i < pipeline_.config().issue_width && workload.has_next(); ++i) {
                instrs.push_back(workload.next());
                pcs.push_back(pc);
                pc += pc_increment;
            }
            
            pipeline_.push_instructions(instrs, pcs);
        }
        
        // Cycle the pipeline
        uint32_t retired = pipeline_.cycle();
        stats.cycles++;
        stats.instructions += retired;
        
        // Progress reporting
        if (progress_callback_ && stats.instructions >= next_progress_) {
            progress_callback_(stats.instructions, stats.cycles);
            next_progress_ += progress_interval_;
        }
    }
    
    // Drain pipeline
    while (!pipeline_.empty()) {
        uint32_t retired = pipeline_.cycle();
        stats.cycles++;
        stats.instructions += retired;
    }
    
    return stats;
}

} // namespace sim