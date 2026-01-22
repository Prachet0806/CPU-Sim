#include "arch/Pipeline.h"
#include <cstddef>


namespace arch {

Pipeline::Pipeline(const PipelineConfig& cfg)
    : config_(cfg),
      stage_busy_(cfg.stages, false),
      stage_cycles_left_(cfg.stages, 0)
{}

uint32_t Pipeline::execute(const Instruction&)
{
    uint32_t stall_cycles = 0;

    // Flush pipeline until stage 0 is free
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

    // Occupy stage 0
    stage_busy_[0] = true;
    stage_cycles_left_[0] = 1;

    // Propagate usage to simulate fullness (simplified model)
    for (size_t i = 1; i < stage_busy_.size(); ++i) {
        stage_busy_[i] = true;
        stage_cycles_left_[i] = 1;
    }

    return stall_cycles + config_.stages;
}

} // namespace arch