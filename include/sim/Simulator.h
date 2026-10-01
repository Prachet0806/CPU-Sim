//include/sim/Simulator.h
#pragma once
#include <cstdint>
#include "arch/Pipeline.h"
#include "arch/Cache.h"
#include "arch/BranchPredictor.h"
#include "workload/Workload.h"

namespace sim {

/**
 * @brief Simulation statistics.
 */
struct SimStats {
    uint64_t cycles = 0;                 ///< Total cycles elapsed
    uint64_t instructions = 0;           ///< Instructions retired
    uint64_t cache_misses = 0;           ///< Cache misses
    uint64_t branch_mispredicts = 0;     ///< Branch mispredictions
    uint64_t cache_hits = 0;             ///< Cache hits
    uint64_t cache_accesses = 0;         ///< Total cache accesses
    uint64_t raw_hazard_stalls = 0;      ///< RAW hazard stall cycles
    uint64_t structural_hazard_stalls = 0; ///< Structural hazard stall cycles
    uint64_t branch_predictions = 0;     ///< Total branch predictions
};

/**
 * @brief Main simulator coordinating pipeline, cache, and branch predictor.
 * 
 * Runs a streaming simulation where instructions are fetched from a Workload,
 * executed through the pipeline, with cache accesses and branch prediction
 * modeled at appropriate pipeline stages.
 */
class Simulator {
public:
    /**
     * @brief Construct simulator with hardware components.
     * @param p Pipeline instance
     * @param c Cache instance
     * @param bp Branch predictor instance
     */
    Simulator(arch::Pipeline& p, arch::Cache& c, arch::BranchPredictor& bp);
    
    /**
     * @brief Run simulation to completion.
     * @param workload Workload providing instruction stream
     * @return Simulation statistics
     */
    SimStats run(workload::Workload& workload);
    
    /**
     * @brief Run simulation with instruction limit.
     * @param workload Workload providing instruction stream
     * @param max_instructions Maximum instructions to simulate
     * @return Simulation statistics
     */
    SimStats run(workload::Workload& workload, uint64_t max_instructions);
    
    /**
     * @brief Set progress callback for long-running simulations.
     * @param cb Callback function (instructions_processed, cycles)
     * @param interval Report interval in instructions
     */
    using ProgressCallback = void(*)(uint64_t instructions_processed, uint64_t cycles);
    void set_progress_callback(ProgressCallback cb, uint64_t interval = 1000000);

private:
    arch::Pipeline&        pipeline_;
    arch::Cache&           cache_;
    arch::BranchPredictor& predictor_;
    
    ProgressCallback progress_callback_ = nullptr;
    uint64_t progress_interval_ = 1000000;
    uint64_t next_progress_ = 1000000;
};

} // namespace sim