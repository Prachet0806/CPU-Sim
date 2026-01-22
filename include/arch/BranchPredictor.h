#pragma once
#include <cstdint>
#include <random>

namespace arch {

struct BranchPredictorConfig {
    float    accuracy;
    uint32_t mispredict_penalty_cycles;
};

class BranchPredictor {
public:
    explicit BranchPredictor(const BranchPredictorConfig& cfg);
    uint32_t predict();

private:
    BranchPredictorConfig config_;
    uint64_t total_predictions_ = 0;
    uint64_t mispredictions_    = 0;
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_;
};

} // namespace arch