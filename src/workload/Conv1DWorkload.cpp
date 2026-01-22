#include "workload/Conv1DWorkload.h"

namespace workload {

const std::vector<arch::Instruction>& Conv1DWorkload::instructions() const {
    return instructions_;
}

Conv1DWorkload::Conv1DWorkload(uint32_t input_len, uint32_t kernel_size) {
    const uint64_t base_input  = 0x40000000;
    const uint64_t base_kernel = 0x50000000;
    const uint64_t base_output = 0x60000000;
    const uint32_t elem_size = 4;
    uint32_t output_len = input_len - kernel_size + 1;

    for (uint32_t i = 0; i < output_len; ++i) {
        instructions_.push_back({arch::InstrType::LOAD, 0, base_output + i * elem_size});
        for (uint32_t k = 0; k < kernel_size; ++k) {
            instructions_.push_back({arch::InstrType::LOAD, 0, base_input + (i + k) * elem_size});
            instructions_.push_back({arch::InstrType::LOAD, 0, base_kernel + k * elem_size});
            instructions_.push_back({arch::InstrType::ALU, 0, 0});
        }
        instructions_.push_back({arch::InstrType::STORE, 0, base_output + i * elem_size});
    }
}

} // namespace workload