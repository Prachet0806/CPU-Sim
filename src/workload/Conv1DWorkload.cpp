//src/workload/Conv1DWorkload.cpp
#include "workload/Conv1DWorkload.h"
#include <cstddef>

namespace workload {

Conv1DWorkload::Conv1DWorkload(uint32_t input_len, uint32_t kernel_size)
    : input_len_(input_len), kernel_size_(kernel_size),
      output_len_(input_len - kernel_size + 1),
      total_instructions_(0), current_instruction_(0),
      i_(0), k_(0), state_(State::LOAD_OUT) {
    
    // Per output element: 1 LOAD_OUT + kernel_size * (2 LOAD + 1 ALU) + 1 STORE
    // = kernel_size * 3 + 2 instructions per output
    total_instructions_ = static_cast<uint64_t>(output_len_) * 
                          (static_cast<uint64_t>(kernel_size_) * 3 + 2);
}

bool Conv1DWorkload::has_next() const {
    return current_instruction_ < total_instructions_;
}

arch::Instruction Conv1DWorkload::next() const {
    if (!has_next()) {
        return {arch::InstrType::ALU, 0, 0};
    }
    
    arch::Instruction instr;
    
    switch (state_) {
        case State::LOAD_OUT: {
            instr = {arch::InstrType::LOAD, 0, OUTPUT_BASE + i_ * ELEM_SIZE};
            break;
        }
        case State::LOAD_IN: {
            instr = {arch::InstrType::LOAD, 0, INPUT_BASE + (i_ + k_) * ELEM_SIZE};
            break;
        }
        case State::LOAD_KERNEL: {
            instr = {arch::InstrType::LOAD, 0, KERNEL_BASE + k_ * ELEM_SIZE};
            break;
        }
        case State::ALU: {
            instr = {arch::InstrType::ALU, 0, 0};
            break;
        }
        case State::STORE_OUT: {
            instr = {arch::InstrType::STORE, 0, OUTPUT_BASE + i_ * ELEM_SIZE};
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

void Conv1DWorkload::advance_state() const {
    switch (state_) {
        case State::LOAD_OUT:
            state_ = State::LOAD_IN;
            k_ = 0;
            break;
        case State::LOAD_IN:
            state_ = State::LOAD_KERNEL;
            break;
        case State::LOAD_KERNEL:
            state_ = State::ALU;
            break;
        case State::ALU:
            ++k_;
            if (k_ < kernel_size_) {
                state_ = State::LOAD_IN;
            } else {
                state_ = State::STORE_OUT;
            }
            break;
        case State::STORE_OUT:
            ++i_;
            if (i_ < output_len_) {
                state_ = State::LOAD_OUT;
            } else {
                state_ = State::DONE;
            }
            break;
        default:
            state_ = State::DONE;
    }
}

void Conv1DWorkload::reset() {
    current_instruction_ = 0;
    i_ = 0;
    k_ = 0;
    state_ = State::LOAD_OUT;
    cache_valid_ = false;
}

std::vector<arch::Instruction> Conv1DWorkload::instructions() const {
    std::vector<arch::Instruction> result;
    result.reserve(static_cast<size_t>(total_instructions_));
    
    // Save current state
    uint32_t save_i = i_, save_k = k_;
    State save_state = state_;
    uint64_t save_current = current_instruction_;
    
    // Reset and generate all
    const_cast<Conv1DWorkload*>(this)->reset();
    while (has_next()) {
        result.push_back(next());
    }
    
    // Restore state
    const_cast<Conv1DWorkload*>(this)->i_ = save_i;
    const_cast<Conv1DWorkload*>(this)->k_ = save_k;
    const_cast<Conv1DWorkload*>(this)->state_ = save_state;
    const_cast<Conv1DWorkload*>(this)->current_instruction_ = save_current;
    
    return result;
}

} // namespace workload