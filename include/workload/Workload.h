#pragma once
#include <vector>
#include "arch/Instruction.h"

namespace workload {

class Workload {
public:
    virtual ~Workload() = default;
    virtual const std::vector<arch::Instruction>& instructions() const = 0;
};

} // namespace workload