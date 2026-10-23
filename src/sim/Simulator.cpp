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
    if (!pipeline_.empty())
    {
        throw std::logic_error("Simulator run requires an empty pipeline");
    }
    SimStats stats{};
    const uint64_t initial_raw_stalls = pipeline_.raw_hazard_stalls();
    const uint64_t initial_structural_stalls = pipeline_.structural_hazard_stalls();
    const uint64_t initial_cache_hits = cache_.get_hits();
    const uint64_t initial_cache_misses = cache_.get_misses();
    const uint64_t initial_predictions = predictor_.total_predictions();
    const uint64_t initial_mispredictions = predictor_.mispredictions();
    uint64_t admitted = 0;
    next_progress_ = progress_interval_;

    // Attach the long-lived components only for this run.
    pipeline_.set_cache_interface(&cache_);
    pipeline_.set_branch_predictor_interface(&predictor_);
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
        // Branch penalties affect the reported total, not in-flight instruction state.
        stats.cycles += pipeline_.branch_penalty_cycles();
        stats.instructions += retired;
        if (progress_callback_ && stats.instructions >= next_progress_)
        {
            progress_callback_(stats.instructions, stats.cycles);
            next_progress_ = stats.instructions + progress_interval_;
        }
    }
    stats.raw_hazard_stalls = pipeline_.raw_hazard_stalls() - initial_raw_stalls;
    stats.structural_hazard_stalls = pipeline_.structural_hazard_stalls() - initial_structural_stalls;
    stats.cache_hits = cache_.get_hits() - initial_cache_hits;
    stats.cache_misses = cache_.get_misses() - initial_cache_misses;
    stats.cache_accesses = stats.cache_hits + stats.cache_misses;
    stats.branch_predictions = predictor_.total_predictions() - initial_predictions;
    stats.branch_mispredicts = predictor_.mispredictions() - initial_mispredictions;
    return stats;
}

} // namespace sim
