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
    explicit CountingCache(uint32_t latency = 10) : latency_(latency) {}
    uint32_t access(uint64_t) override
    {
        ++accesses;
        ++this_cycle;
        if (this_cycle > max_in_cycle)
            max_in_cycle = this_cycle;
        start_cycles.push_back(current_cycle);
        return latency_;
    }
    uint32_t hit_latency() const override { return 1; }
    uint32_t miss_latency() const override { return latency_; }
    void new_cycle(uint32_t cycle = 0)
    {
        this_cycle = 0;
        current_cycle = cycle;
    }
    uint32_t accesses = 0;
    uint32_t this_cycle = 0;
    uint32_t max_in_cycle = 0;
    uint32_t current_cycle = 0;
    std::vector<uint32_t> start_cycles;

  private:
    uint32_t latency_;
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
    CHECK(cache.start_cycles.size() == 2);
}

void test_blocking_cache_and_exact_latency()
{
    for (uint32_t latency : {1U, 2U, 10U})
    {
        arch::Pipeline pipeline({5, 2});
        CountingCache cache(latency);
        pipeline.set_cache_interface(&cache);
        CHECK(pipeline.push_instructions({{arch::InstrType::LOAD, 0, 0x1000}}, {0}) == 1);
        uint32_t retired = 0;
        uint32_t cycles = 0;
        while (!pipeline.empty() && cycles < 100)
        {
            cache.new_cycle(cycles);
            retired += pipeline.cycle();
            ++cycles;
        }
        CHECK(pipeline.empty());
        CHECK(retired == 1);
        CHECK(cache.start_cycles.size() == 1);
        CHECK(cache.start_cycles[0] == 3);
        CHECK(cycles == latency + 4);
    }

    arch::Pipeline pipeline({5, 2});
    CountingCache cache(10);
    pipeline.set_cache_interface(&cache);
    CHECK(pipeline.push_instructions({{arch::InstrType::LOAD, 0, 0x1000}, {arch::InstrType::LOAD, 0, 0x2000}},
                                     {0, 4}) == 2);
    uint32_t retired = 0;
    for (uint32_t cycle = 0; cycle < 100 && !pipeline.empty(); ++cycle)
    {
        cache.new_cycle(cycle);
        retired += pipeline.cycle();
    }
    CHECK(pipeline.empty());
    CHECK(retired == 2);
    CHECK(cache.start_cycles.size() == 2);
    CHECK(cache.start_cycles[1] - cache.start_cycles[0] == 10);
    CHECK(cache.max_in_cycle == 1);
}

void test_cache_hit_miss_timing_and_flush()
{
    arch::Pipeline pipeline({5, 1});
    arch::Cache cache({1024, 64, 1, 1, 10});
    arch::BranchPredictor predictor({1.0f, 0});
    sim::Simulator simulator(pipeline, cache, predictor);
    SequenceWorkload workload({{arch::InstrType::LOAD, 0, 0x1000}});
    const auto miss = simulator.run(workload);
    CHECK(miss.cache_misses == 1);
    CHECK(miss.cycles == 14);
    workload.reset();
    const auto hit = simulator.run(workload);
    CHECK(hit.cache_hits == 1);
    CHECK(hit.cycles == 5);

    CountingCache blocking(10);
    pipeline.set_cache_interface(&blocking);
    CHECK(pipeline.push_instructions({{arch::InstrType::LOAD, 0, 0x2000}}, {0}) == 1);
    for (uint32_t cycle = 0; cycle < 4; ++cycle)
    {
        blocking.new_cycle(cycle);
        CHECK(pipeline.cycle() == 0);
    }
    CHECK(blocking.accesses == 1);
    pipeline.flush();
    CHECK(pipeline.empty());
    CHECK(pipeline.push_instructions({{arch::InstrType::LOAD, 0, 0x3000}}, {4}) == 1);
    uint32_t retired = 0;
    for (uint32_t cycle = 4; cycle < 100 && !pipeline.empty(); ++cycle)
    {
        blocking.new_cycle(cycle);
        retired += pipeline.cycle();
    }
    CHECK(pipeline.empty());
    CHECK(retired == 1);
    CHECK(blocking.accesses == 2);
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
    for (uint32_t width : {1U, 2U, 4U})
    {
        workload::BranchWorkload correct(5);
        workload::BranchWorkload incorrect(5);
        const auto hit = run_workload(correct, width);
        const auto miss = run_workload(incorrect, width, true, 7);
        CHECK(hit.instructions == 10);
        CHECK(miss.instructions == 10);
        CHECK(miss.branch_predictions == 5);
        CHECK(miss.branch_mispredicts == 5);
        CHECK(miss.cycles == hit.cycles + 35);
    }

    workload::BranchWorkload zero(3);
    const auto zero_stats = run_workload(zero, 2, true, 0);
    CHECK(zero_stats.branch_mispredicts == 3);
    CHECK(zero_stats.instructions == 6);
}

void test_superscalar_throughput()
{
    const std::vector<arch::Instruction> alus(32, {arch::InstrType::ALU, 0, 0});
    uint64_t previous_cycles = UINT64_MAX;
    for (uint32_t width : {1U, 2U, 4U})
    {
        SequenceWorkload workload(alus);
        const auto stats = run_workload(workload, width);
        CHECK(stats.instructions == alus.size());
        CHECK(stats.cycles == 32 / width + 4);
        CHECK(stats.cycles < previous_cycles);
        previous_cycles = stats.cycles;
    }

    std::vector<arch::Instruction> mixed(8, {arch::InstrType::ALU, 0, 0});
    mixed.push_back({arch::InstrType::LOAD, 0, 0x1000});
    mixed.insert(mixed.end(), 8, {arch::InstrType::ALU, 0, 0});
    SequenceWorkload narrow(mixed);
    SequenceWorkload wide(mixed);
    const auto narrow_stats = run_workload(narrow, 1);
    const auto wide_stats = run_workload(wide, 2);
    CHECK(narrow_stats.instructions == mixed.size());
    CHECK(wide_stats.instructions == mixed.size());
    CHECK(narrow_stats.cache_accesses == 1);
    CHECK(wide_stats.cache_accesses == 1);
    CHECK(wide_stats.cycles < narrow_stats.cycles);
}

void test_repeated_simulator_runs()
{
    arch::Pipeline pipeline({5, 2});
    arch::Cache cache({1024, 64, 1, 1, 10});
    arch::BranchPredictor predictor({1.0f, 3});
    predictor.set_force_mispredict(true);
    sim::Simulator simulator(pipeline, cache, predictor);
    SequenceWorkload workload({{arch::InstrType::LOAD, 0, 0x1000}, {arch::InstrType::BRANCH, 0, 0}});
    const auto first = simulator.run(workload);
    CHECK(pipeline.empty());
    CHECK(first.instructions == 2);
    CHECK(first.cache_accesses == 1);
    CHECK(first.cache_misses == 1);
    CHECK(first.branch_predictions == 1);
    CHECK(first.branch_mispredicts == 1);

    workload.reset();
    const auto second = simulator.run(workload);
    CHECK(pipeline.empty());
    CHECK(second.instructions == 2);
    CHECK(second.cache_accesses == 1);
    CHECK(second.cache_hits == 1);
    CHECK(second.cache_misses == 0);
    CHECK(second.branch_predictions == 1);
    CHECK(second.branch_mispredicts == 1);
    CHECK(second.cycles < first.cycles);
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
    test_blocking_cache_and_exact_latency();
    test_cache_hit_miss_timing_and_flush();
    test_order_and_raw_hazard();
    test_stall_statistics();
    test_instruction_conservation();
    test_branch_penalty_and_zero_penalty_stats();
    test_superscalar_throughput();
    test_repeated_simulator_runs();
    test_bounds();
    std::cout << "All architecture tests passed.\n";
    return 0;
}
