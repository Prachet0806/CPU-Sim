#include "arch/Pipeline.h"
#include <cstddef>
#include <cassert>


namespace arch {

Pipeline::Pipeline(const PipelineConfig& cfg)
    : config_(cfg),
      stage_busy_(cfg.stages, false),
      stage_cycles_left_(cfg.stages, 0)
{
    // Note: Full multi-issue (issue_width > 1) would require tracking multiple
    // instructions per stage. Current implementation uses issue_width to model
    // average throughput improvement (simplified model).
    assert(cfg.issue_width > 0 && "Issue width must be positive");
}

void Pipeline::flush() {
    // Clear all pipeline stages (used on branch misprediction)
    for (size_t i = 0; i < stage_busy_.size(); ++i) {
        stage_busy_[i] = false;
        stage_cycles_left_[i] = 0;
    }
}

uint32_t Pipeline::execute(const Instruction& instr)
{
    uint32_t stall_cycles = 0;

    // Wait until stage 0 is free (structural hazard)
    while (stage_busy_[0]) {
        ++stall_cycles;
        for (size_t i = 0; i < stage_cycles_left_.size(); ++i) {
            if (stage_cycles_left_[i] > 0) {
                --stage_cycles_left_[i];
                if (stage_cycles_left_[i] == 0)
                    stage_busy_[i] = false;
            }
        }
    }

    // Use instruction-specific latency (default is 0 for most instructions)
    // This allows modeling multi-cycle operations like divide, multiply
    uint32_t base_cycles = config_.stages + instr.latency;
    
    // Apply issue width: with N-issue width, sustained throughput is N instructions per cycle
    // Simplified model: amortize pipeline latency over issue width for sustained execution
    // (assumes no resource conflicts after initial fill)
    uint32_t effective_cycles = stall_cycles + base_cycles;
    if (config_.issue_width > 1 && stall_cycles == 0) {
        // After pipeline is filled, amortize cost across issue width
        effective_cycles = stall_cycles + (base_cycles + config_.issue_width - 1) / config_.issue_width;
    }
    
    // Occupy stage 0
    stage_busy_[0] = true;
    stage_cycles_left_[0] = 1;

    // Propagate through pipeline stages (simplified in-order model)
    for (size_t i = 1; i < stage_busy_.size(); ++i) {
        stage_busy_[i] = true;
        stage_cycles_left_[i] = 1;
    }

    return effective_cycles;
}

} // namespace arch