#include "workload/Conv1DWorkload.h"

namespace workload {

const std::vector<arch::Instruction>& Conv1DWorkload::instructions() const {
    return instructions_;
}

Conv1DWorkload::Conv1DWorkload(uint32_t input_len, uint32_t kernel_size) {
    // Memory base addresses for convolution buffers (simulated virtual addresses)
    constexpr uint64_t INPUT_BASE  = 0x40000000;  // 1 GB
    constexpr uint64_t KERNEL_BASE = 0x50000000;  // 1.25 GB
    constexpr uint64_t OUTPUT_BASE = 0x60000000;  // 1.5 GB
    constexpr uint32_t ELEM_SIZE = 4;  // 4 bytes per element (float32)
    uint32_t output_len = input_len - kernel_size + 1;

    for (uint32_t i = 0; i < output_len; ++i) {
        instructions_.push_back({arch::InstrType::LOAD, 0, OUTPUT_BASE + i * ELEM_SIZE});
        for (uint32_t k = 0; k < kernel_size; ++k) {
            instructions_.push_back({arch::InstrType::LOAD, 0, INPUT_BASE + (i + k) * ELEM_SIZE});
            instructions_.push_back({arch::InstrType::LOAD, 0, KERNEL_BASE + k * ELEM_SIZE});
            instructions_.push_back({arch::InstrType::ALU, 0, 0});
        }
        instructions_.push_back({arch::InstrType::STORE, 0, OUTPUT_BASE + i * ELEM_SIZE});
    }
}

} // namespace workload