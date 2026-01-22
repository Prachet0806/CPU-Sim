#include <iostream>
#include <cassert>
#include <cstdlib>
#include "arch/Cache.h"
#include "arch/BranchPredictor.h"
#include "sim/Simulator.h"

// Simple test helper macro
#define ASSERT_EQ(val, expected) \
    if ((val) != (expected)) { \
        std::cerr << "[FAIL] " << __FUNCTION__ << ": Expected " << (expected) \
                  << " but got " << (val) << "\n"; \
        std::exit(1); \
    } else { \
        std::cout << "[PASS] " << __FUNCTION__ << "\n"; \
    }

void test_cache_hit_logic() {
    // Config: 1KB Cache, Direct Mapped (Associativity=1)
    arch::CacheConfig cfg{1024, 64, 1, 1, 10};
    arch::Cache cache(cfg);

    uint64_t addr = 0x1000;

    // 1. First Access -> Cold Miss
    uint32_t lat1 = cache.access(addr);
    ASSERT_EQ(lat1, cfg.miss_latency);

    // 2. Second Access -> Hit
    uint32_t lat2 = cache.access(addr);
    ASSERT_EQ(lat2, cfg.hit_latency);
}

void test_cache_conflict_eviction() {
    // Config: Small cache (128B), Line=64B -> Only 2 sets (Set 0, Set 1)
    // Associativity=1 -> Direct Mapped
    arch::CacheConfig cfg{128, 64, 1, 1, 10}; 
    arch::Cache cache(cfg);

    uint64_t addr_A = 0x0000; // Maps to Set 0
    uint64_t addr_B = 0x1000; // Maps to Set 0 (Conflict!)

    // Fill A
    cache.access(addr_A); 
    // Fill B (Evicts A because assoc=1)
    cache.access(addr_B); 

    // Access A again -> Should be Miss (Conflict Miss)
    uint32_t lat = cache.access(addr_A);
    ASSERT_EQ(lat, cfg.miss_latency);
}

void test_branch_penalty() {
    // Config: 0% Accuracy -> Always fail
    arch::BranchPredictorConfig cfg{0.0f, 20}; 
    arch::BranchPredictor bp(cfg);

    // With 0% accuracy, every prediction must mispredict
    uint32_t penalty = bp.predict();
    ASSERT_EQ(penalty, 20);
}

int main() {
    std::cout << "=== Running Architecture Unit Tests ===\n";
    test_cache_hit_logic();
    test_cache_conflict_eviction();
    test_branch_penalty();
    std::cout << "All Unit Tests Passed.\n";
    return 0;
}