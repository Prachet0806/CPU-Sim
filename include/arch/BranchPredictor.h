//include/arch/BranchPredictor.h
#pragma once
#include <cstdint>
#include <random>

namespace arch {

/**
 * @brief Configuration parameters for the branch predictor.
 */
struct BranchPredictorConfig {
    float    accuracy;                  ///< Prediction accuracy (0.0 to 1.0)
    uint32_t mispredict_penalty_cycles; ///< Penalty in cycles on misprediction
};

/**
 * @brief Probabilistic branch predictor.
 * 
 * Models a branch predictor with configurable accuracy using a random number
 * generator with fixed seed for deterministic replay. On each prediction,
 * generates a random number and compares against accuracy threshold.
 */
class BranchPredictor {
public:
    /**
     * @brief Construct branch predictor with given configuration.
     * @param cfg Branch predictor configuration
     */
    explicit BranchPredictor(const BranchPredictorConfig& cfg);
    
    // Prevent copying and moving (RNG state should not be duplicated)
    BranchPredictor(const BranchPredictor&) = delete;
    BranchPredictor& operator=(const BranchPredictor&) = delete;
    BranchPredictor(BranchPredictor&&) = delete;
    BranchPredictor& operator=(BranchPredictor&&) = delete;
    
    /**
     * @brief Predict branch outcome.
     * @return Penalty cycles (0 if correct, mispredict_penalty_cycles if mispredicted)
     */
    uint32_t predict();

private:
    BranchPredictorConfig config_;
    uint64_t total_predictions_ = 0;
    uint64_t mispredictions_    = 0;
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_;
};

} // namespace arch