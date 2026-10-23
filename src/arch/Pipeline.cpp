#include "arch/Pipeline.h"

#include <algorithm>
#include <stdexcept>

namespace arch
{

Pipeline::Pipeline(const PipelineConfig& cfg) : config_(cfg)
{
    if (cfg.stages != 5 || cfg.issue_width == 0 || cfg.issue_width > 8)
    {
        throw std::invalid_argument("Pipeline requires five stages and issue width 1-8");
    }
    stages_.resize(cfg.stages);
}

size_t Pipeline::available_fetch_slots() const
{
    size_t free_slots = 0;
    for (size_t slot = 0; slot < config_.issue_width; ++slot)
    {
        if (!stages_[static_cast<size_t>(PipelineStage::FETCH)][slot].valid)
        {
            ++free_slots;
        }
    }
    return free_slots;
}

bool Pipeline::can_accept() const { return available_fetch_slots() != 0; }

size_t Pipeline::push_instructions(const std::vector<Instruction>& instrs, const std::vector<uint64_t>& pcs)
{
    if (instrs.size() != pcs.size() || instrs.size() > config_.issue_width)
    {
        throw std::invalid_argument("Invalid instruction bundle");
    }

    size_t accepted = 0;
    auto& fetch = stages_[static_cast<size_t>(PipelineStage::FETCH)];
    for (size_t slot = 0; slot < config_.issue_width && accepted < instrs.size(); ++slot)
    {
        if (fetch[slot].valid)
        {
            continue;
        }
        PipelineInstruction entry{};
        entry.instr = instrs[accepted];
        entry.pc = pcs[accepted];
        entry.sequence = next_sequence_++;
        entry.valid = true;
        if (entry.instr.type == InstrType::BRANCH && branch_predictor_)
        {
            entry.branch_penalty_cycles = branch_predictor_->predict(entry.pc);
        }
        fetch[slot] = entry;
        ++accepted;
    }
    return accepted;
}

bool Pipeline::has_older_at_or_before(size_t stage_idx, uint64_t sequence) const
{
    for (size_t stage = 0; stage <= stage_idx; ++stage)
    {
        for (size_t slot = 0; slot < config_.issue_width; ++slot)
        {
            const auto& other = stages_[stage][slot];
            if (other.valid && other.sequence < sequence)
            {
                return true;
            }
        }
    }
    return false;
}

bool Pipeline::has_older_store(uint64_t sequence, uint64_t address) const
{
    for (const auto& stage : stages_)
    {
        for (size_t slot = 0; slot < config_.issue_width; ++slot)
        {
            const auto& other = stage[slot];
            if (other.valid && other.sequence < sequence && other.instr.type == InstrType::STORE &&
                other.instr.memory_address == address)
            {
                return true;
            }
        }
    }
    return false;
}

uint32_t Pipeline::cycle()
{
    branch_penalty_cycles_ = 0;
    uint32_t retired = 0;

    // A younger instruction may reach writeback, but cannot retire first.
    auto& writeback = stages_[static_cast<size_t>(PipelineStage::WRITEBACK)];
    for (size_t count = 0; count < config_.issue_width; ++count)
    {
        auto it = std::find_if(writeback.begin(), writeback.begin() + config_.issue_width,
                               [this](const PipelineInstruction& entry)
                               { return entry.valid && entry.sequence == next_retire_sequence_; });
        if (it == writeback.begin() + config_.issue_width)
        {
            break;
        }
        it->valid = false;
        ++next_retire_sequence_;
        ++retired;
    }

    // A blocking transaction occupies the port through its completion cycle.
    bool memory_port_busy_this_cycle = active_memory_sequence_.has_value();
    for (int stage_index = static_cast<int>(PipelineStage::MEMORY); stage_index >= 0; --stage_index)
    {
        const size_t stage = static_cast<size_t>(stage_index);
        std::vector<size_t> order;
        for (size_t slot = 0; slot < config_.issue_width; ++slot)
        {
            if (stages_[stage][slot].valid)
            {
                order.push_back(slot);
            }
        }
        std::sort(order.begin(), order.end(), [this, stage](size_t left, size_t right)
                  { return stages_[stage][left].sequence < stages_[stage][right].sequence; });

        for (size_t slot : order)
        {
            auto& entry = stages_[stage][slot];
            if (!entry.valid)
            {
                continue;
            }
            ++entry.cycles_in_stage;

            if (stage == static_cast<size_t>(PipelineStage::MEMORY) && entry.instr.type == InstrType::LOAD &&
                has_older_store(entry.sequence, entry.instr.memory_address))
            {
                ++raw_hazard_stalls_;
                continue;
            }
            if (has_older_at_or_before(stage, entry.sequence))
            {
                ++structural_hazard_stalls_;
                continue;
            }

            switch (static_cast<PipelineStage>(stage_index))
            {
            case PipelineStage::FETCH:
                if (entry.instr.type == InstrType::BRANCH && !entry.branch_penalty_applied)
                {
                    branch_penalty_cycles_ += entry.branch_penalty_cycles;
                    entry.branch_penalty_applied = true;
                }
                advance_instruction(stage, slot);
                break;
            case PipelineStage::DECODE:
                advance_instruction(stage, slot);
                break;
            case PipelineStage::EXECUTE:
                if (entry.cycles_in_stage >= 1ULL + entry.instr.latency)
                {
                    advance_instruction(stage, slot);
                }
                break;
            case PipelineStage::MEMORY:
                if (entry.instr.type == InstrType::LOAD || entry.instr.type == InstrType::STORE)
                {
                    if (!entry.memory_access_done)
                    {
                        if (memory_port_busy_this_cycle)
                        {
                            ++structural_hazard_stalls_;
                            break;
                        }
                        if (!cache_)
                        {
                            throw std::logic_error("Memory instruction requires a cache");
                        }
                        entry.memory_latency = cache_->access(entry.instr.memory_address);
                        if (entry.memory_latency == 0)
                        {
                            throw std::logic_error("Cache access latency must be positive");
                        }
                        entry.memory_access_done = true;
                        entry.cycles_in_stage = 1;
                        active_memory_sequence_ = entry.sequence;
                        memory_port_busy_this_cycle = true;
                    }
                    if (entry.memory_access_done && entry.cycles_in_stage >= entry.memory_latency &&
                        advance_instruction(stage, slot))
                    {
                        active_memory_sequence_.reset();
                    }
                }
                else
                {
                    advance_instruction(stage, slot);
                }
                break;
            case PipelineStage::WRITEBACK:
                break;
            }
        }
    }
    return retired;
}

bool Pipeline::advance_instruction(size_t stage_idx, size_t slot)
{
    if (stage_idx + 1 >= stages_.size())
    {
        return false;
    }
    auto& next = stages_[stage_idx + 1];
    if (stage_idx + 1 == static_cast<size_t>(PipelineStage::MEMORY))
    {
        const auto& moving = stages_[stage_idx][slot];
        if (moving.instr.type == InstrType::LOAD && has_older_store(moving.sequence, moving.instr.memory_address))
        {
            ++raw_hazard_stalls_;
            return false;
        }
        for (size_t other_slot = 0; other_slot < config_.issue_width; ++other_slot)
        {
            const auto& other = next[other_slot];
            if (other.valid && other.memory_access_done &&
                (other.instr.type == InstrType::LOAD || other.instr.type == InstrType::STORE))
            {
                ++structural_hazard_stalls_;
                return false;
            }
        }
    }
    auto it = std::find_if(next.begin(), next.begin() + config_.issue_width,
                           [](const PipelineInstruction& entry) { return !entry.valid; });
    if (it == next.begin() + config_.issue_width)
    {
        ++structural_hazard_stalls_;
        return false;
    }
    *it = stages_[stage_idx][slot];
    it->cycles_in_stage = 0;
    stages_[stage_idx][slot].valid = false;
    return true;
}

void Pipeline::flush()
{
    for (auto& stage : stages_)
    {
        for (auto& entry : stage)
        {
            entry = PipelineInstruction{};
        }
    }
    next_retire_sequence_ = next_sequence_;
    branch_penalty_cycles_ = 0;
    active_memory_sequence_.reset();
}

bool Pipeline::empty() const
{
    for (const auto& stage : stages_)
    {
        for (size_t slot = 0; slot < config_.issue_width; ++slot)
        {
            if (stage[slot].valid)
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace arch
