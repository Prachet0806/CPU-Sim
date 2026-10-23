// include/arch/Pipeline.h
#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <cstddef>
#include <optional>
#include "Instruction.h"

namespace arch
{

/**
 * @brief Configuration parameters for the pipeline.
 */
struct PipelineConfig
{
    uint32_t stages = 5;      ///< Number of pipeline stages (default 5: F, D, E, M, W)
    uint32_t issue_width = 1; ///< Instructions issued per cycle (superscalar)
};

/**
 * @brief Pipeline stage enumeration.
 */
enum class PipelineStage : uint32_t
{
    FETCH = 0,
    DECODE = 1,
    EXECUTE = 2,
    MEMORY = 3,
    WRITEBACK = 4
};

/**
 * @brief Instruction in the pipeline with execution state.
 */
struct PipelineInstruction
{
    Instruction instr;                  ///< The instruction being executed
    uint64_t pc = 0;                    ///< Program counter for branch prediction
    uint64_t sequence = 0;              ///< Admission order for in-order progress
    bool valid = false;                 ///< Whether this slot holds a valid instruction
    uint64_t cycles_in_stage = 0;       ///< Cycles spent in current stage, including the current cycle
    bool memory_access_done = false;    ///< For MEM stage tracking
    uint32_t memory_latency = 0;        ///< Latency from cache access
    uint32_t branch_penalty_cycles = 0; ///< Abstract cycle penalty for a mispredicted branch
    bool branch_penalty_applied = false;
};

/**
 * @brief Abstract interface for cache access from pipeline.
 */
class CacheInterface
{
  public:
    virtual ~CacheInterface() = default;
    /**
     * @brief Access cache at given address.
     * @param address Memory address to access
     * @return Positive latency in cycles, including the access-start cycle
     */
    virtual uint32_t access(uint64_t address) = 0;
    virtual uint32_t hit_latency() const = 0;
    virtual uint32_t miss_latency() const = 0;
};

/**
 * @brief Abstract interface for branch prediction from pipeline.
 */
class BranchPredictorInterface
{
  public:
    virtual ~BranchPredictorInterface() = default;
    /**
     * @brief Predict branch outcome.
     * @param pc Program counter of branch instruction
     * @return Penalty cycles (0 if predicted correctly, mispredict_penalty if mispredicted)
     */
    virtual uint32_t predict(uint64_t pc) = 0;
};

/**
 * @brief In-order pipeline simulator with configurable stages and issue width.
 *
 * Models a classic 5-stage pipeline (Fetch, Decode, Execute, Memory, Writeback)
 * with support for superscalar execution, data hazard detection (RAW),
 * structural hazard detection (single memory port), and branch prediction.
 */
class Pipeline
{
  public:
    /**
     * @brief Construct pipeline with given configuration.
     * @param cfg Pipeline configuration (stages, issue_width)
     */
    explicit Pipeline(const PipelineConfig& cfg);

    // Prevent copying and moving (pipeline state should not be duplicated)
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;

    /**
     * @brief Check if pipeline can accept new instructions in fetch stage.
     * @return true if fetch stage has free slots
     */
    bool can_accept() const;

    /**
     * @brief Push new instructions into fetch stage.
     * @param instrs Vector of instructions to push (up to issue_width)
     * @param pcs Corresponding program counters for branch prediction
     */
    size_t push_instructions(const std::vector<Instruction>& instrs, const std::vector<uint64_t>& pcs);
    size_t available_fetch_slots() const;

    /**
     * @brief Advance pipeline by one cycle.
     * @return Number of instructions retired this cycle
     */
    uint32_t cycle();

    /**
     * @brief Manually discard all in-flight instructions.
     *
     * Branch mispredictions do not call this method. Lifetime stall-event
     * counters, cache state, and predictor state are not reset.
     */
    void flush();

    /**
     * @brief Get pipeline depth.
     * @return Number of stages
     */
    uint32_t depth() const { return config_.stages; }

    /**
     * @brief Check if pipeline is empty.
     * @return true if no valid instructions in pipeline
     */
    bool empty() const;

    /**
     * @brief Get abstract branch penalty cycles assigned during the last cycle.
     * @return Penalty cycles to add to the reported total (0 if none)
     */
    uint64_t branch_penalty_cycles() const { return branch_penalty_cycles_; }

    /** @brief Legacy name for branch_penalty_cycles(); no pipeline flush occurs. */
    uint64_t flush_penalty_cycles() const { return branch_penalty_cycles(); }

    /**
     * @brief Get accumulated RAW hazard instruction-cycle stall events.
     * @return One event per blocked instruction per cycle, not elapsed cycles
     */
    uint64_t raw_hazard_stalls() const { return raw_hazard_stalls_; }

    /**
     * @brief Get accumulated structural hazard instruction-cycle stall events.
     * @return One event per blocked instruction per cycle, not elapsed cycles
     */
    uint64_t structural_hazard_stalls() const { return structural_hazard_stalls_; }

    /**
     * @brief Set cache interface for memory stage.
     * @param cache Pointer to cache interface implementation
     */
    void set_cache_interface(CacheInterface* cache) { cache_ = cache; }

    /**
     * @brief Set branch predictor interface for fetch stage.
     * @param bp Pointer to branch predictor interface implementation
     */
    void set_branch_predictor_interface(BranchPredictorInterface* bp) { branch_predictor_ = bp; }

    const PipelineConfig& config() const { return config_; }

  private:
    PipelineConfig config_;
    std::vector<std::array<PipelineInstruction, 8>> stages_; // Max issue_width 8

    CacheInterface* cache_ = nullptr;
    BranchPredictorInterface* branch_predictor_ = nullptr;

    uint64_t branch_penalty_cycles_ = 0;    ///< Abstract branch penalty assigned in the last cycle
    uint64_t raw_hazard_stalls_ = 0;        ///< Accumulated RAW instruction-cycle stall events
    uint64_t structural_hazard_stalls_ = 0; ///< Accumulated structural instruction-cycle stall events
    uint64_t next_sequence_ = 0;
    uint64_t next_retire_sequence_ = 0;
    std::optional<uint64_t> active_memory_sequence_; ///< The sole outstanding blocking-cache access

    bool advance_instruction(size_t stage_idx, size_t issue_slot);
    bool has_older_at_or_before(size_t stage_idx, uint64_t sequence) const;
    bool has_older_store(uint64_t sequence, uint64_t address) const;
};

} // namespace arch
