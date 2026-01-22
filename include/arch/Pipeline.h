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
    uint32_t execute(const Instruction& instr);

private:
    PipelineConfig config_;
    std::vector<bool>     stage_busy_;
    std::vector<uint32_t> stage_cycles_left_;
};

} // namespace arch