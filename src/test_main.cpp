#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "arch/BranchPredictor.h"
#include "arch/Cache.h"
#include "arch/Pipeline.h"
#include "sim/Simulator.h"
#include "workload/BranchWorkload.h"
#include "workload/Conv1DWorkload.h"
#include "workload/MatMulWorkload.h"

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::cerr << "[FAIL] " << __func__ << " line " << __LINE__ << ": " #condition "\n";                        \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)

class CountingCache : public arch::CacheInterface
{
  public:
    uint32_t access(uint64_t) override
    {
        ++accesses;
        ++this_cycle;
        if (this_cycle > max_in_cycle)
            max_in_cycle = this_cycle;
        return 10;
    }
    uint32_t hit_latency() const override { return 1; }
    uint32_t miss_latency() const override { return 10; }
    void new_cycle() { this_cycle = 0; }
    uint32_t accesses = 0;
    uint32_t this_cycle = 0;
    uint32_t max_in_cycle = 0;
};

class SequenceWorkload : public workload::Workload
{
  public:
    explicit SequenceWorkload(std::vector<arch::Instruction> instructions) : instructions_(std::move(instructions)) {}
    bool has_next() const override { return next_ < instructions_.size(); }
    arch::Instruction next() const override { return instructions_[next_++]; }
    void reset() override { next_ = 0; }

  private:
    std::vector<arch::Instruction> instructions_;
    mutable size_t next_ = 0;
};

void test_cache()
{
    arch::Cache cache({128, 64, 1, 1, 10});
    CHECK(cache.access(0) == 10);
    CHECK(cache.access(0) == 1);
    CHECK(cache.access(0x1000) == 10);
    CHECK(cache.access(0) == 10);
    CHECK(cache.get_accesses() == 4);
    bool rejected = false;
    try
    {
        arch::Cache invalid({128, 0, 1, 1, 10});
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    CHECK(rejected);
}

void test_pipeline_admission_and_port()
{
    arch::Pipeline pipeline({5, 2});
    CountingCache cache;
    pipeline.set_cache_interface(&cache);
    const std::vector<arch::Instruction> loads = {{arch::InstrType::LOAD, 0, 0x1000},
                                                  {arch::InstrType::LOAD, 0, 0x2000}};
    CHECK(pipeline.available_fetch_slots() == 2);
    CHECK(pipeline.push_instructions(loads, {0, 4}) == 2);
    CHECK(pipeline.available_fetch_slots() == 0);
    CHECK(pipeline.push_instructions({{arch::InstrType::ALU, 0, 0}}, {8}) == 0);
    uint32_t retired = 0;
    for (int cycle = 0; cycle < 100 && !pipeline.empty(); ++cycle)
    {
        cache.new_cycle();
        retired += pipeline.cycle();
    }
    CHECK(pipeline.empty());
    CHECK(retired == 2);
    CHECK(cache.accesses == 2);
    CHECK(cache.max_in_cycle == 1);
}

void test_order_and_raw_hazard()
{
    arch::Pipeline pipeline({5, 2});
    CountingCache cache;
    pipeline.set_cache_interface(&cache);
    CHECK(pipeline.push_instructions({{arch::InstrType::STORE, 0, 0x1000}, {arch::InstrType::LOAD, 0, 0x1000}},
                                     {0, 4}) == 2);
    uint32_t retired = 0;
    for (int cycle = 0; cycle < 100 && !pipeline.empty(); ++cycle)
    {
        cache.new_cycle();
        const uint32_t just_retired = pipeline.cycle();
        if (cycle < 10)
            CHECK(just_retired == 0);
        retired += just_retired;
    }
    CHECK(pipeline.empty());
    CHECK(retired == 2);
    CHECK(pipeline.raw_hazard_stalls() > 0);
    CHECK(cache.accesses == 2);

    arch::Pipeline independent({5, 2});
    CountingCache independent_cache;
    independent.set_cache_interface(&independent_cache);
    CHECK(independent.push_instructions({{arch::InstrType::LOAD, 0, 0x1000}, {arch::InstrType::ALU, 0, 0}}, {0, 4}) ==
          2);
    retired = 0;
    for (int cycle = 0; cycle < 100 && !independent.empty(); ++cycle)
    {
        independent_cache.new_cycle();
        const uint32_t just_retired = independent.cycle();
        if (cycle < 10)
            CHECK(just_retired == 0);
        retired += just_retired;
    }
    CHECK(retired == 2);
    CHECK(independent.empty());
}

void test_stall_statistics()
{
    arch::Pipeline pipeline({5, 2});
    arch::Cache cache({1024, 64, 1, 1, 10});
    arch::BranchPredictor predictor({1.0f, 7});
    sim::Simulator simulator(pipeline, cache, predictor);
    SequenceWorkload workload({{arch::InstrType::STORE, 0, 0x1000}, {arch::InstrType::LOAD, 0, 0x1000}});
    const auto stats = simulator.run(workload);
    CHECK(stats.instructions == 2);
    CHECK(stats.raw_hazard_stalls > 0);
    CHECK(stats.raw_hazard_stalls == pipeline.raw_hazard_stalls());
    CHECK(stats.structural_hazard_stalls == pipeline.structural_hazard_stalls());
}

sim::SimStats run_workload(workload::Workload& workload, uint32_t width, bool mispredict = false, uint32_t penalty = 7,
                           uint64_t limit = UINT64_MAX)
{
    arch::Pipeline pipeline({5, width});
    arch::Cache cache({1024, 64, 1, 1, 10});
    arch::BranchPredictor predictor({1.0f, penalty});
    if (mispredict)
        predictor.set_force_mispredict(true);
    sim::Simulator simulator(pipeline, cache, predictor);
    return simulator.run(workload, limit);
}

void test_instruction_conservation()
{
    for (uint32_t width : {1U, 2U, 8U})
    {
        workload::MatMulWorkload matmul(2);
        const auto matmul_stats = run_workload(matmul, width);
        CHECK(matmul_stats.instructions == 32);
        CHECK(matmul_stats.cache_hits + matmul_stats.cache_misses == matmul_stats.cache_accesses);

        workload::Conv1DWorkload conv(4, 2);
        CHECK(run_workload(conv, width).instructions == 24);

        workload::BranchWorkload branches(4);
        const auto branch_stats = run_workload(branches, width, true);
        CHECK(branch_stats.instructions == 8);
        CHECK(branch_stats.branch_predictions == 4);
        CHECK(branch_stats.branch_mispredicts == 4);

        workload::MatMulWorkload limited(2);
        CHECK(run_workload(limited, width, false, 7, 3).instructions == 3);
        CHECK(limited.has_next());
    }
}

void test_branch_penalty_and_zero_penalty_stats()
{
    workload::BranchWorkload correct(3);
    workload::BranchWorkload incorrect(3);
    const auto hit = run_workload(correct, 2);
    const auto miss = run_workload(incorrect, 2, true, 7);
    CHECK(hit.instructions == 6);
    CHECK(miss.instructions == 6);
    CHECK(miss.branch_mispredicts == 3);
    CHECK(miss.cycles == hit.cycles + 21);

    workload::BranchWorkload zero(3);
    const auto zero_stats = run_workload(zero, 2, true, 0);
    CHECK(zero_stats.branch_mispredicts == 3);
    CHECK(zero_stats.instructions == 6);
}

void test_bounds()
{
    bool rejected = false;
    try
    {
        arch::Pipeline invalid({5, 9});
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    CHECK(rejected);
    rejected = false;
    try
    {
        workload::Conv1DWorkload invalid(3, 4);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    CHECK(rejected);
    rejected = false;
    try
    {
        arch::BranchPredictor invalid({std::numeric_limits<float>::quiet_NaN(), 7});
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    CHECK(rejected);
}

int main()
{
    test_cache();
    test_pipeline_admission_and_port();
    test_order_and_raw_hazard();
    test_stall_statistics();
    test_instruction_conservation();
    test_branch_penalty_and_zero_penalty_stats();
    test_bounds();
    std::cout << "All architecture tests passed.\n";
    return 0;
}
