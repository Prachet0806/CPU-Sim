//include/arch/Pipeline.h
#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <optional>
#include "Instruction.h"

namespace arch {

/**
 * @brief Configuration parameters for the pipeline.
 */
struct PipelineConfig {
    uint32_t stages = 5;       ///< Number of pipeline stages (default 5: F, D, E, M, W)
    uint32_t issue_width = 1;  ///< Instructions issued per cycle (superscalar)
};

/**
 * @brief Pipeline stage enumeration.
 */
enum class PipelineStage : uint32_t {
    FETCH = 0,
    DECODE = 1,
    EXECUTE = 2,
    MEMORY = 3,
    WRITEBACK = 4
};

/**
 * @brief Instruction in the pipeline with execution state.
 */
struct PipelineInstruction {
    Instruction instr;                    ///< The instruction being executed
    uint64_t pc = 0;                      ///< Program counter for branch prediction
    bool valid = false;                   ///< Whether this slot holds a valid instruction
    uint32_t cycles_in_stage = 0;         ///< Cycles spent in current stage
    bool memory_access_done = false;      ///< For MEM stage tracking
    uint32_t memory_latency = 0;          ///< Latency from cache access or mispredict penalty
};

/**
 * @brief Abstract interface for cache access from pipeline.
 */
class CacheInterface {
public:
    virtual ~CacheInterface() = default;
    /**
     * @brief Access cache at given address.
     * @param address Memory address to access
     * @return Latency in cycles (hit_latency or miss_latency)
     */
    virtual uint32_t access(uint64_t address) = 0;
    virtual uint32_t hit_latency() const = 0;
    virtual uint32_t miss_latency() const = 0;
};

/**
 * @brief Abstract interface for branch prediction from pipeline.
 */
class BranchPredictorInterface {
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
class Pipeline {
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
    void push_instructions(const std::vector<Instruction>& instrs, const std::vector<uint64_t>& pcs);
    
    /**
     * @brief Advance pipeline by one cycle.
     * @return Number of instructions retired this cycle
     */
    uint32_t cycle();
    
    /**
     * @brief Flush pipeline (e.g., on branch misprediction).
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
    std::vector<std::array<PipelineInstruction, 8>> stages_;  // Max issue_width 8
    
    bool memory_port_busy_ = false;
    uint32_t memory_port_cycles_left_ = 0;
    
    std::vector<uint64_t> pending_load_addresses_;
    std::vector<uint64_t> pending_store_addresses_;
    
    CacheInterface* cache_ = nullptr;
    BranchPredictorInterface* branch_predictor_ = nullptr;
    
    void advance_instruction(size_t stage_idx, size_t issue_slot);
    bool has_raw_hazard(uint64_t address) const;
    bool has_memory_structural_hazard() const;
};

} // namespace arch