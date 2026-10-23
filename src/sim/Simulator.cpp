// src/sim/Simulator.cpp
#include "sim/Simulator.h"
#include "arch/Pipeline.h"
#include "arch/Cache.h"
#include "arch/BranchPredictor.h"
#include <cstdint>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace sim
{

// Cache adapter implementing Pipeline's CacheInterface
class CacheAdapter : public arch::CacheInterface
{
  public:
    explicit CacheAdapter(arch::Cache& cache, SimStats* stats) : cache_(cache), stats_(stats) {}

    uint32_t access(uint64_t address) override
    {
        uint32_t latency = cache_.access(address);
        if (stats_)
        {
            ++stats_->cache_accesses;
            if (latency == cache_.miss_latency())
            {
                ++stats_->cache_misses;
            }
            else
            {
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
class BranchPredictorAdapter : public arch::BranchPredictorInterface
{
  public:
    explicit BranchPredictorAdapter(arch::BranchPredictor& bp, SimStats* stats) : bp_(bp), stats_(stats) {}

    uint32_t predict(uint64_t pc) override
    {
        if (stats_)
            ++stats_->branch_predictions;
        uint32_t penalty = bp_.predict(pc);
        if (bp_.last_mispredicted() && stats_)
            ++stats_->branch_mispredicts;
        return penalty;
    }

  private:
    arch::BranchPredictor& bp_;
    SimStats* stats_;
};

Simulator::Simulator(arch::Pipeline& p, arch::Cache& c, arch::BranchPredictor& bp)
    : pipeline_(p), cache_(c), predictor_(bp)
{
}

void Simulator::set_progress_callback(ProgressCallback cb, uint64_t interval)
{
    if (interval == 0)
    {
        throw std::invalid_argument("Progress interval must be positive");
    }
    progress_callback_ = cb;
    progress_interval_ = interval;
    next_progress_ = interval;
}

SimStats Simulator::run(workload::Workload& workload) { return run(workload, UINT64_MAX); }

SimStats Simulator::run(workload::Workload& workload, uint64_t max_instructions)
{
    SimStats stats{};
    const uint64_t initial_raw_stalls = pipeline_.raw_hazard_stalls();
    const uint64_t initial_structural_stalls = pipeline_.structural_hazard_stalls();
    uint64_t admitted = 0;
    next_progress_ = progress_interval_;

    // Create adapters
    CacheAdapter cache_adapter(cache_, &stats);
    BranchPredictorAdapter bp_adapter(predictor_, &stats);

    // Set adapters in pipeline
    pipeline_.set_cache_interface(&cache_adapter);
    pipeline_.set_branch_predictor_interface(&bp_adapter);
    struct InterfaceReset
    {
        arch::Pipeline& pipeline;
        ~InterfaceReset()
        {
            pipeline.set_cache_interface(nullptr);
            pipeline.set_branch_predictor_interface(nullptr);
        }
    } interface_reset{pipeline_};

    // Program counter for branch prediction
    uint64_t pc = 0;
    const uint64_t pc_increment = 4; // Assume 4-byte instructions

    // Streaming execution
    while ((workload.has_next() && admitted < max_instructions) || !pipeline_.empty())
    {
        const size_t available = pipeline_.available_fetch_slots();
        if (available != 0 && workload.has_next() && admitted < max_instructions)
        {
            std::vector<arch::Instruction> instrs;
            std::vector<uint64_t> pcs;
            const uint64_t remaining = max_instructions - admitted;
            const size_t count = static_cast<size_t>(std::min<uint64_t>(available, remaining));
            instrs.reserve(count);
            pcs.reserve(count);
            for (size_t i = 0; i < count && workload.has_next(); ++i)
            {
                instrs.push_back(workload.next());
                pcs.push_back(pc);
                pc += pc_increment;
            }
            const size_t accepted = pipeline_.push_instructions(instrs, pcs);
            if (accepted != instrs.size())
            {
                throw std::logic_error("Pipeline rejected a reserved instruction slot");
            }
            admitted += static_cast<uint64_t>(accepted);
        }

        // Cycle the pipeline
        uint32_t retired = pipeline_.cycle();
        stats.cycles++;
        // Add branch misprediction flush penalty cycles (not counted as instructions)
        stats.cycles += pipeline_.flush_penalty_cycles();
        stats.instructions += retired;
        if (progress_callback_ && stats.instructions >= next_progress_)
        {
            progress_callback_(stats.instructions, stats.cycles);
            next_progress_ = stats.instructions + progress_interval_;
        }
    }
    stats.raw_hazard_stalls = pipeline_.raw_hazard_stalls() - initial_raw_stalls;
    stats.structural_hazard_stalls = pipeline_.structural_hazard_stalls() - initial_structural_stalls;
    return stats;
}

} // namespace sim
