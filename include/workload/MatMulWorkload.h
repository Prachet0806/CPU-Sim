//include/workload/MatMulWorkload.h
#pragma once
#include "Workload.h"
#include <cstdint>
#include <vector>

namespace workload {

/**
 * @brief Matrix multiplication workload (GEMM).
 * 
 * Generates instruction stream for dense matrix multiplication C = A * B
 * with N x N matrices. Uses row-major layout with column-strided access
 * to B matrix, creating challenging cache access patterns.
 * 
 * Memory layout:
 * - Matrix A: base 0x10000000, row-major
 * - Matrix B: base 0x20000000, row-major (column access = stride-N)
 * - Matrix C: base 0x30000000, row-major
 * - Element size: 4 bytes (float32)
 */
class MatMulWorkload : public Workload {
public:
    /**
     * @brief Construct MatMul workload.
     * @param N Matrix dimension (N x N matrices)
     */
    explicit MatMulWorkload(uint32_t N);
    
    bool has_next() const override;
    arch::Instruction next() const override;
    void reset() override;
    
    // Legacy interface
    std::vector<arch::Instruction> instructions() const override;

private:
    uint32_t N_;
    uint64_t total_instructions_;
    mutable uint64_t current_instruction_;
    
    mutable uint32_t i_, j_, k_;
    mutable enum class State { LOAD_C, LOAD_A, LOAD_B, ALU, STORE_C, DONE } state_;
    
    static constexpr uint64_t MATRIX_A_BASE = 0x10000000;
    static constexpr uint64_t MATRIX_B_BASE = 0x20000000;
    static constexpr uint64_t MATRIX_C_BASE = 0x30000000;
    static constexpr uint32_t ELEM_SIZE = 4;
    
    mutable std::vector<arch::Instruction> cached_instructions_;
    mutable bool cache_valid_ = false;
    
    void advance_state() const;
};

} // namespace workload