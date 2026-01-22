#pragma once
#include "Workload.h"
#include <vector>

namespace workload {

class Conv1DWorkload : public Workload {
public:
    Conv1DWorkload(uint32_t input_len, uint32_t kernel_size);
    const std::vector<arch::Instruction>& instructions() const override;
private:
    std::vector<arch::Instruction> instructions_;
};

} // namespace workload