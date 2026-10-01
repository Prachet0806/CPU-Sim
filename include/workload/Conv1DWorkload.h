//include/workload/Conv1DWorkload.h
#pragma once
#include "Workload.h"
#include <cstdint>
#include <vector>

namespace workload {

/**
 * @brief 1D Convolution workload.
 * 
 * Generates instruction stream for 1D convolution (sliding window):
 * output[i] = sum_{k=0}^{K-1} input[i+k] * kernel[k]
 * 
 * Memory layout:
 * - Input:  base 0x40000000
 * - Kernel: base 0x50000000
 * - Output: base 0x60000000
 * - Element size: 4 bytes (float32)
 * 
 * Access pattern shows high temporal reuse of kernel weights
 * and streaming access to input/output buffers.
 */
class Conv1DWorkload : public Workload {
public:
    /**
     * @brief Construct Conv1D workload.
     * @param input_len Length of input array
     * @param kernel_size Size of convolution kernel
     */
    Conv1DWorkload(uint32_t input_len, uint32_t kernel_size);
    
    bool has_next() const override;
    arch::Instruction next() const override;
    void reset() override;
    
    // Legacy interface
    std::vector<arch::Instruction> instructions() const override;

private:
    uint32_t input_len_;
    uint32_t kernel_size_;
    uint32_t output_len_;
    uint64_t total_instructions_;
    mutable uint64_t current_instruction_;
    
    mutable uint32_t i_, k_;
    mutable enum class State { LOAD_OUT, LOAD_IN, LOAD_KERNEL, ALU, STORE_OUT, DONE } state_;
    
    static constexpr uint64_t INPUT_BASE  = 0x40000000;
    static constexpr uint64_t KERNEL_BASE = 0x50000000;
    static constexpr uint64_t OUTPUT_BASE = 0x60000000;
    static constexpr uint32_t ELEM_SIZE = 4;
    
    mutable std::vector<arch::Instruction> cached_instructions_;
    mutable bool cache_valid_ = false;
    
    void advance_state() const;
};

} // namespace workload