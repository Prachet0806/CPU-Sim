#include "arch/BranchPredictor.h"

namespace arch {

BranchPredictor::BranchPredictor(const BranchPredictorConfig& cfg)
    : config_(cfg), rng_(12345), dist_(0.0f, 1.0f) {}

uint32_t BranchPredictor::predict() {
    ++total_predictions_;
    if (dist_(rng_) > config_.accuracy) {
        ++mispredictions_;
        return config_.mispredict_penalty_cycles;
    }
    return 0;
}

} // namespace arch