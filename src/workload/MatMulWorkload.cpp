#include "workload/MatMulWorkload.h"

namespace workload {

const std::vector<arch::Instruction>& MatMulWorkload::instructions() const {
    return instructions_;
}

MatMulWorkload::MatMulWorkload(uint32_t N) {
    const uint64_t base_A = 0x10000000;
    const uint64_t base_B = 0x20000000;
    const uint64_t base_C = 0x30000000;
    const uint32_t elem   = 4;

    instructions_.reserve(N * N * N * 5); // Optimization

    for (uint32_t i = 0; i < N; ++i)
        for (uint32_t j = 0; j < N; ++j) {
            instructions_.push_back({arch::InstrType::LOAD, 0, base_C + (i*N + j)*elem});
            for (uint32_t k = 0; k < N; ++k) {
                instructions_.push_back({arch::InstrType::LOAD, 0, base_A + (i*N + k)*elem});
                instructions_.push_back({arch::InstrType::LOAD, 0, base_B + (k*N + j)*elem}); // Stride!
                instructions_.push_back({arch::InstrType::ALU,  0, 0});
            }
            instructions_.push_back({arch::InstrType::STORE, 0, base_C + (i*N + j)*elem});
        }
}

} // namespace workload