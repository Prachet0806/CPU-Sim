// src/arch/BranchPredictor.cpp
#include "arch/BranchPredictor.h"
#include <cmath>
#include <stdexcept>

namespace arch
{

BranchPredictor::BranchPredictor(const BranchPredictorConfig& cfg) : config_(cfg), rng_(12345), dist_(0.0f, 1.0f)
{
    if (!std::isfinite(cfg.accuracy) || cfg.accuracy < 0.0f || cfg.accuracy > 1.0f)
    {
        throw std::invalid_argument("Branch prediction accuracy must be in [0, 1]");
    }
}

uint32_t BranchPredictor::predict(uint64_t pc)
{
    (void)pc;
    ++total_predictions_;
    last_mispredicted_ = false;
    if (force_mispredict_)
    {
        ++mispredictions_;
        last_mispredicted_ = true;
        return config_.mispredict_penalty_cycles;
    }
    if (force_hit_)
    {
        return 0;
    }
    if (dist_(rng_) > config_.accuracy)
    {
        ++mispredictions_;
        last_mispredicted_ = true;
        return config_.mispredict_penalty_cycles;
    }
    return 0;
}

void BranchPredictor::set_force_mispredict(bool force)
{
    force_mispredict_ = force;
    if (force)
        force_hit_ = false;
}

void BranchPredictor::set_force_hit(bool force)
{
    force_hit_ = force;
    if (force)
        force_mispredict_ = false;
}

} // namespace arch
