#pragma once
#include <cstdint>

namespace arch {

enum class InstrType {
    ALU,
    LOAD,
    STORE,
    BRANCH
};

struct Instruction {
    InstrType type;
    uint32_t  latency;        // workload-specified (usually 0)
    uint64_t  memory_address; // valid for LOAD/STORE
};

} // namespace arch