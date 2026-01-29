#include "workload/MatMulWorkload.h"

namespace workload {

const std::vector<arch::Instruction>& MatMulWorkload::instructions() const {
    return instructions_;
}

MatMulWorkload::MatMulWorkload(uint32_t N) {
    // Memory base addresses for matrices (simulated virtual addresses)
    // These are spaced to avoid aliasing in typical cache configurations
    constexpr uint64_t MATRIX_A_BASE = 0x10000000;  // 256 MB
    constexpr uint64_t MATRIX_B_BASE = 0x20000000;  // 512 MB
    constexpr uint64_t MATRIX_C_BASE = 0x30000000;  // 768 MB
    constexpr uint32_t ELEM_SIZE = 4;  // 4 bytes per element (float32)

    // Use size_t to prevent overflow for large N values
    // Each iteration generates: 1 LOAD (C) + N*(2 LOAD + 1 ALU) + 1 STORE = N*3 + 2 instructions
    size_t total_instrs = static_cast<size_t>(N) * N * (static_cast<size_t>(N) * 3 + 2);
    instructions_.reserve(total_instrs);

    for (uint32_t i = 0; i < N; ++i)
        for (uint32_t j = 0; j < N; ++j) {
            instructions_.push_back({arch::InstrType::LOAD, 0, MATRIX_C_BASE + (i*N + j)*ELEM_SIZE});
            for (uint32_t k = 0; k < N; ++k) {
                instructions_.push_back({arch::InstrType::LOAD, 0, MATRIX_A_BASE + (i*N + k)*ELEM_SIZE});
                instructions_.push_back({arch::InstrType::LOAD, 0, MATRIX_B_BASE + (k*N + j)*ELEM_SIZE}); // Column stride!
                instructions_.push_back({arch::InstrType::ALU,  0, 0});
            }
            instructions_.push_back({arch::InstrType::STORE, 0, MATRIX_C_BASE + (i*N + j)*ELEM_SIZE});
        }
}

} // namespace workload