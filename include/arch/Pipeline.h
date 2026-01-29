#pragma once
#include <cstdint>
#include <vector>
#include "Instruction.h"

namespace arch {

struct PipelineConfig {
    uint32_t stages;
    uint32_t issue_width;
};

class Pipeline {
public:
    explicit Pipeline(const PipelineConfig& cfg);
    
    // Prevent copying and moving (pipeline state should not be duplicated)
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;
    
    uint32_t execute(const Instruction& instr);
    void flush();  // Clear pipeline on branch misprediction

private:
    PipelineConfig config_;
    std::vector<bool>     stage_busy_;
    std::vector<uint32_t> stage_cycles_left_;
};

} // namespace arch