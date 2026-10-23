// src/workload/BranchWorkload.cpp
#include "workload/BranchWorkload.h"
#include <cstddef>

namespace workload
{

BranchWorkload::BranchWorkload(uint64_t branch_count)
    : branch_count_(branch_count), total_instructions_(branch_count * 2), current_instruction_(0), branch_turn_(false)
{
}

bool BranchWorkload::has_next() const { return current_instruction_ < total_instructions_; }

arch::Instruction BranchWorkload::next() const
{
    if (!has_next())
    {
        return {arch::InstrType::ALU, 0, 0};
    }

    arch::Instruction instr;
    if (branch_turn_)
    {
        instr = {arch::InstrType::BRANCH, 0, 0xDEADBEEF};
    }
    else
    {
        instr = {arch::InstrType::ALU, 0, 0};
    }

    advance_state();
    ++current_instruction_;
    return instr;
}

void BranchWorkload::advance_state() const { branch_turn_ = !branch_turn_; }

void BranchWorkload::reset()
{
    current_instruction_ = 0;
    branch_turn_ = false;
    cache_valid_ = false;
}

std::vector<arch::Instruction> BranchWorkload::instructions() const
{
    std::vector<arch::Instruction> result;
    result.reserve(static_cast<size_t>(total_instructions_));

    // Save current state
    bool save_branch_turn = branch_turn_;
    uint64_t save_current = current_instruction_;

    // Reset and generate all
    const_cast<BranchWorkload*>(this)->reset();
    while (has_next())
    {
        result.push_back(next());
    }

    // Restore state
    const_cast<BranchWorkload*>(this)->branch_turn_ = save_branch_turn;
    const_cast<BranchWorkload*>(this)->current_instruction_ = save_current;

    return result;
}

} // namespace workload
