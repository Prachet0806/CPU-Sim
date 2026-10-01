//src/workload/MatMulWorkload.cpp
#include "workload/MatMulWorkload.h"
#include <cstddef>

namespace workload {

MatMulWorkload::MatMulWorkload(uint32_t N)
    : N_(N), total_instructions_(0), current_instruction_(0),
      i_(0), j_(0), k_(0), state_(State::LOAD_C) {
    
    // Calculate total instructions: N*N * (1 LOAD_C + N*(2 LOAD + 1 ALU) + 1 STORE)
    // = N^2 * (N*3 + 2)
    total_instructions_ = static_cast<uint64_t>(N_) * N_ * 
                          (static_cast<uint64_t>(N_) * 3 + 2);
}

bool MatMulWorkload::has_next() const {
    return current_instruction_ < total_instructions_;
}

arch::Instruction MatMulWorkload::next() const {
    if (!has_next()) {
        return {arch::InstrType::ALU, 0, 0};  // Return dummy
    }
    
    // Generate instruction based on current state
    arch::Instruction instr;
    
    switch (state_) {
        case State::LOAD_C: {
            instr = {arch::InstrType::LOAD, 0, 
                     MATRIX_C_BASE + (i_ * N_ + j_) * ELEM_SIZE};
            break;
        }
        case State::LOAD_A: {
            instr = {arch::InstrType::LOAD, 0,
                     MATRIX_A_BASE + (i_ * N_ + k_) * ELEM_SIZE};
            break;
        }
        case State::LOAD_B: {
            // Column stride access - causes cache conflicts
            instr = {arch::InstrType::LOAD, 0,
                     MATRIX_B_BASE + (k_ * N_ + j_) * ELEM_SIZE};
            break;
        }
        case State::ALU: {
            instr = {arch::InstrType::ALU, 0, 0};
            break;
        }
        case State::STORE_C: {
            instr = {arch::InstrType::STORE, 0,
                     MATRIX_C_BASE + (i_ * N_ + j_) * ELEM_SIZE};
            break;
        }
        default: {
            instr = {arch::InstrType::ALU, 0, 0};
        }
    }
    
    advance_state();
    ++current_instruction_;
    return instr;
}

void MatMulWorkload::advance_state() const {
    switch (state_) {
        case State::LOAD_C:
            state_ = State::LOAD_A;
            k_ = 0;
            break;
        case State::LOAD_A:
            state_ = State::LOAD_B;
            break;
        case State::LOAD_B:
            state_ = State::ALU;
            break;
        case State::ALU:
            ++k_;
            if (k_ < N_) {
                state_ = State::LOAD_A;
            } else {
                state_ = State::STORE_C;
            }
            break;
        case State::STORE_C:
            ++j_;
            if (j_ < N_) {
                state_ = State::LOAD_C;
            } else {
                j_ = 0;
                ++i_;
                if (i_ < N_) {
                    state_ = State::LOAD_C;
                } else {
                    state_ = State::DONE;
                }
            }
            break;
        default:
            state_ = State::DONE;
    }
}

void MatMulWorkload::reset() {
    current_instruction_ = 0;
    i_ = 0;
    j_ = 0;
    k_ = 0;
    state_ = State::LOAD_C;
    cache_valid_ = false;
}

std::vector<arch::Instruction> MatMulWorkload::instructions() const {
    std::vector<arch::Instruction> result;
    result.reserve(static_cast<size_t>(total_instructions_));
    
    // Save current state
    uint32_t save_i = i_, save_j = j_, save_k = k_;
    State save_state = state_;
    uint64_t save_current = current_instruction_;
    
    // Reset and generate all
    const_cast<MatMulWorkload*>(this)->reset();
    while (has_next()) {
        result.push_back(next());
    }
    
    // Restore state
    const_cast<MatMulWorkload*>(this)->i_ = save_i;
    const_cast<MatMulWorkload*>(this)->j_ = save_j;
    const_cast<MatMulWorkload*>(this)->k_ = save_k;
    const_cast<MatMulWorkload*>(this)->state_ = save_state;
    const_cast<MatMulWorkload*>(this)->current_instruction_ = save_current;
    
    return result;
}

} // namespace workload