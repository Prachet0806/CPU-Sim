//include/arch/Instruction.h
#pragma once
#include <cstdint>

namespace arch {

/**
 * @brief Instruction types supported by the simulator.
 */
enum class InstrType {
    ALU,    ///< Arithmetic/logic operation (no memory access)
    LOAD,   ///< Load from memory
    STORE,  ///< Store to memory
    BRANCH  ///< Conditional branch
};

/**
 * @brief Instruction representation.
 * 
 * Lightweight instruction descriptor containing type, optional latency,
 * and memory address for load/store instructions.
 */
struct Instruction {
    InstrType type;         ///< Instruction type
    uint32_t  latency;      ///< Additional latency cycles (e.g., for multi-cycle ALU ops)
    uint64_t  memory_address; ///< Memory address for LOAD/STORE (ignored for ALU/BRANCH)
};

} // namespace arch