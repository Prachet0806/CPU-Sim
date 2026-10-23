// include/workload/BranchWorkload.h
#pragma once
#include "Workload.h"
#include <cstdint>
#include <vector>

namespace workload
{

/**
 * @brief Branch-heavy workload for exercising branch prediction.
 *
 * Generates a stream of alternating ALU and BRANCH instructions so that the
 * branch predictor, pipeline flush on misprediction, and branch statistics
 * can be exercised and measured.
 *
 * Memory layout:
 * - Instructions are purely synthetic; BRANCH instructions carry a dummy
 *   memory address (ignored by the pipeline).
 */
class BranchWorkload : public Workload
{
  public:
    /**
     * @brief Construct BranchWorkload.
     * @param branch_count Number of BRANCH instructions to emit (each paired
     *                     with a preceding ALU instruction)
     */
    explicit BranchWorkload(uint64_t branch_count);

    bool has_next() const override;
    arch::Instruction next() const override;
    void reset() override;

    // Legacy interface
    std::vector<arch::Instruction> instructions() const override;

  private:
    uint64_t branch_count_;
    uint64_t total_instructions_;
    mutable uint64_t current_instruction_;
    mutable bool branch_turn_; // true => next is BRANCH, false => ALU

    mutable std::vector<arch::Instruction> cached_instructions_;
    mutable bool cache_valid_ = false;

    void advance_state() const;
};

} // namespace workload
