#pragma once
#include "Workload.h"
#include <vector>

namespace workload {

class MatMulWorkload : public Workload {
public:
    explicit MatMulWorkload(uint32_t N);
    const std::vector<arch::Instruction>& instructions() const override;
private:
    std::vector<arch::Instruction> instructions_;
};

} // namespace workload