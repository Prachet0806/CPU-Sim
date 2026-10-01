//include/workload/Workload.h
#pragma once
#include <vector>
#include "arch/Instruction.h"

namespace workload {

/**
 * @brief Abstract base class for workloads.
 * 
 * Provides a streaming interface for instruction generation, allowing
 * simulation of arbitrarily large workloads without materializing
 * the entire instruction stream in memory.
 */
class Workload {
public:
    virtual ~Workload() = default;
    
    /**
     * @brief Check if more instructions are available.
     * @return true if next() will return a valid instruction
     */
    virtual bool has_next() const = 0;
    
    /**
     * @brief Get next instruction in the stream.
     * @return Next instruction (undefined if has_next() is false)
     */
    virtual arch::Instruction next() const = 0;
    
    /**
     * @brief Get all instructions as a vector (legacy interface).
     * @note Materializes entire instruction stream in memory.
     *       Use streaming interface (has_next/next) for large workloads.
     * @return Vector of all instructions
     */
    virtual std::vector<arch::Instruction> instructions() const {
        return {};
    }
    
    /**
     * @brief Reset workload to initial state for re-running.
     */
    virtual void reset() = 0;
};

} // namespace workload