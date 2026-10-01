//src/arch/pipeline.cpp
#include "arch/Pipeline.h"
#include <cassert>
#include <algorithm>

namespace arch {

Pipeline::Pipeline(const PipelineConfig& cfg)
    : config_(cfg),
      stages_(cfg.stages)
{
    assert(cfg.stages > 0 && "Pipeline must have at least 1 stage");
    assert(cfg.stages <= 10 && "Pipeline stages limited to 10");
    assert(cfg.issue_width > 0 && "Issue width must be positive");
    assert(cfg.issue_width <= 8 && "Issue width limited to 8");
    
    // Initialize all stages
    for (auto& stage : stages_) {
        for (auto& slot : stage) {
            slot.valid = false;
            slot.cycles_in_stage = 0;
            slot.memory_access_done = false;
            slot.memory_latency = 0;
        }
    }
}

bool Pipeline::can_accept() const {
    // Check if fetch stage has any free slots
    for (size_t i = 0; i < config_.issue_width; ++i) {
        if (!stages_[static_cast<size_t>(PipelineStage::FETCH)][i].valid) {
            return true;
        }
    }
    return false;
}

void Pipeline::push_instructions(const std::vector<Instruction>& instrs, const std::vector<uint64_t>& pcs) {
    assert(instrs.size() == pcs.size());
    assert(instrs.size() <= config_.issue_width);
    
    size_t fetch_stage = static_cast<size_t>(PipelineStage::FETCH);
    size_t slot = 0;
    
    for (size_t i = 0; i < instrs.size(); ++i) {
        // Find next free slot
        while (slot < config_.issue_width && stages_[fetch_stage][slot].valid) {
            ++slot;
        }
        if (slot >= config_.issue_width) break;
        
        // Check for RAW hazard before pushing LOAD/STORE
        if ((instrs[i].type == InstrType::LOAD || instrs[i].type == InstrType::STORE) &&
            has_raw_hazard(instrs[i].memory_address)) {
            // Stall - don't push this instruction yet
            break;
        }
        
        stages_[fetch_stage][slot] = PipelineInstruction{};
        stages_[fetch_stage][slot].instr = instrs[i];
        stages_[fetch_stage][slot].pc = pcs[i];
        stages_[fetch_stage][slot].valid = true;
        stages_[fetch_stage][slot].cycles_in_stage = 1;
        
        // Branch prediction at fetch stage
        if (instrs[i].type == InstrType::BRANCH && branch_predictor_) {
            uint32_t penalty = branch_predictor_->predict(pcs[i]);
            if (penalty > 0) {
                // Mark for flush on misprediction
                stages_[fetch_stage][slot].memory_latency = penalty;
            }
        }
        
        ++slot;
    }
}

uint32_t Pipeline::cycle() {
    uint32_t retired = 0;
    
    // Process stages in reverse order (WRITEBACK -> FETCH)
    for (int stage_idx = static_cast<int>(config_.stages) - 1; stage_idx >= 0; --stage_idx) {
        for (size_t slot = 0; slot < config_.issue_width; ++slot) {
            auto& instr = stages_[stage_idx][slot];
            if (!instr.valid) continue;
            
            // Increment cycles in current stage
            instr.cycles_in_stage++;
            
            PipelineStage current_stage = static_cast<PipelineStage>(stage_idx);
            
            switch (current_stage) {
                case PipelineStage::FETCH: {
                    // Fetch takes 1 cycle, then moves to DECODE
                    if (instr.cycles_in_stage >= 1) {
                        // Check for branch misprediction
                        if (instr.instr.type == InstrType::BRANCH && instr.memory_latency > 0) {
                            // Mispredicted - flush pipeline
                            flush();
                            retired = 0;  // No retirement on flush cycle
                            // Add misprediction penalty cycles
                            return instr.memory_latency;
                        }
                        advance_instruction(stage_idx, slot);
                    }
                    break;
                }
                case PipelineStage::DECODE: {
                    // Decode takes 1 cycle
                    if (instr.cycles_in_stage >= 1) {
                        advance_instruction(stage_idx, slot);
                    }
                    break;
                }
                case PipelineStage::EXECUTE: {
                    // ALU ops take 1 cycle (plus instr.latency)
                    // Memory ops will need MEM stage
                    uint32_t execute_cycles = 1 + instr.instr.latency;
                    if (instr.cycles_in_stage >= execute_cycles) {
                        advance_instruction(stage_idx, slot);
                    }
                    break;
                }
                case PipelineStage::MEMORY: {
                    // Memory access for LOAD/STORE
                    if (instr.instr.type == InstrType::LOAD || instr.instr.type == InstrType::STORE) {
                        if (!instr.memory_access_done && cache_) {
                            // Start memory access
                            instr.memory_latency = cache_->access(instr.instr.memory_address);
                            instr.memory_access_done = true;
                            instr.cycles_in_stage = 0;  // Reset cycles for memory wait
                            
                            // Track pending addresses for RAW hazard
                            if (instr.instr.type == InstrType::LOAD) {
                                pending_load_addresses_.push_back(instr.instr.memory_address);
                            } else {
                                pending_store_addresses_.push_back(instr.instr.memory_address);
                            }
                        }
                        
                        // Wait for memory latency cycles
                        if (instr.cycles_in_stage < instr.memory_latency) {
                            // Still waiting for memory
                        } else {
                            // Memory access complete
                            advance_instruction(stage_idx, slot);
                            
                            // Remove from pending addresses
                            if (instr.instr.type == InstrType::LOAD) {
                                auto it = std::find(pending_load_addresses_.begin(), 
                                                    pending_load_addresses_.end(), 
                                                    instr.instr.memory_address);
                                if (it != pending_load_addresses_.end()) {
                                    pending_load_addresses_.erase(it);
                                }
                            } else {
                                auto it = std::find(pending_store_addresses_.begin(), 
                                                    pending_store_addresses_.end(), 
                                                    instr.instr.memory_address);
                                if (it != pending_store_addresses_.end()) {
                                    pending_store_addresses_.erase(it);
                                }
                            }
                            instr.memory_access_done = false;
                        }
                    } else {
                        // Non-memory instruction passes through MEM in 1 cycle
                        if (instr.cycles_in_stage >= 1) {
                            advance_instruction(stage_idx, slot);
                        }
                    }
                    break;
                }
                case PipelineStage::WRITEBACK: {
                    // Writeback takes 1 cycle, then instruction retires
                    if (instr.cycles_in_stage >= 1) {
                        instr.valid = false;
                        retired++;
                    }
                    break;
                }
            }
        }
    }
    
    return retired;
}

void Pipeline::advance_instruction(size_t stage_idx, size_t slot) {
    if (stage_idx + 1 >= stages_.size()) {
        // Should not happen - WRITEBACK handled separately
        return;
    }
    
    auto& instr = stages_[stage_idx][slot];
    
    // Check structural hazard for next stage (MEM stage has single port)
    if (stage_idx + 1 == static_cast<size_t>(PipelineStage::MEMORY)) {
        // Check if any instruction in MEM stage is doing memory access
        bool mem_port_busy = false;
        for (size_t s = 0; s < config_.issue_width; ++s) {
            auto& mem_instr = stages_[stage_idx + 1][s];
            if (mem_instr.valid && 
                (mem_instr.instr.type == InstrType::LOAD || mem_instr.instr.type == InstrType::STORE) &&
                mem_instr.memory_access_done) {
                mem_port_busy = true;
                break;
            }
        }
        if (mem_port_busy) {
            return;  // Stall - memory port busy
        }
    }
    
    // Find free slot in next stage
    size_t next_slot = 0;
    while (next_slot < config_.issue_width && stages_[stage_idx + 1][next_slot].valid) {
        ++next_slot;
    }
    if (next_slot >= config_.issue_width) {
        return;  // Next stage full - stall
    }
    
    // Move instruction to next stage
    stages_[stage_idx + 1][next_slot] = instr;
    stages_[stage_idx + 1][next_slot].cycles_in_stage = 0;
    stages_[stage_idx][slot].valid = false;
}

bool Pipeline::has_raw_hazard(uint64_t address) const {
    // Check if any pending LOAD or STORE has the same address
    for (uint64_t addr : pending_load_addresses_) {
        if (addr == address) return true;
    }
    for (uint64_t addr : pending_store_addresses_) {
        if (addr == address) return true;
    }
    return false;
}

void Pipeline::flush() {
    // Clear all pipeline stages
    for (auto& stage : stages_) {
        for (auto& slot : stage) {
            slot.valid = false;
            slot.cycles_in_stage = 0;
            slot.memory_access_done = false;
            slot.memory_latency = 0;
        }
    }
    // Clear pending addresses
    pending_load_addresses_.clear();
    pending_store_addresses_.clear();
}

bool Pipeline::empty() const {
    for (const auto& stage : stages_) {
        for (const auto& slot : stage) {
            if (slot.valid) return false;
        }
    }
    return true;
}

} // namespace arch