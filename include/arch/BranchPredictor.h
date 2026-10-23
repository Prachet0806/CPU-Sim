// include/arch/BranchPredictor.h
#pragma once
#include <cstdint>
#include <random>
#include "arch/Pipeline.h"

namespace arch
{

/**
 * @brief Configuration parameters for the branch predictor.
 */
struct BranchPredictorConfig
{
    float accuracy;                     ///< Prediction accuracy (0.0 to 1.0)
    uint32_t mispredict_penalty_cycles; ///< Penalty in cycles on misprediction
};

/**
 * @brief Probabilistic branch predictor.
 *
 * Models a branch predictor with configurable accuracy using a random number
 * generator with fixed seed for deterministic replay. On each prediction,
 * generates a random number and compares against accuracy threshold.
 */
class BranchPredictor : public BranchPredictorInterface
{
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
     * @param pc Program counter of the branch instruction
     * @return Penalty cycles (0 if correct, mispredict_penalty_cycles if mispredicted)
     */
    uint32_t predict(uint64_t pc);
    bool last_mispredicted() const { return last_mispredicted_; }
    uint64_t total_predictions() const { return total_predictions_; }
    uint64_t mispredictions() const { return mispredictions_; }

    /**
     * @brief Force every prediction to mispredict (test hook).
     * @param force When true, every call to predict() returns the mispredict penalty.
     */
    void set_force_mispredict(bool force);

    /**
     * @brief Force every prediction to hit (test hook).
     * @param force When true, every call to predict() returns 0 penalty.
     */
    void set_force_hit(bool force);

  private:
    BranchPredictorConfig config_;
    uint64_t total_predictions_ = 0;
    uint64_t mispredictions_ = 0;
    bool force_mispredict_ = false;
    bool force_hit_ = false;
    bool last_mispredicted_ = false;
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_;
};

} // namespace arch
