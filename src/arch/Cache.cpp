#include "arch/Cache.h"
#include <cassert>
#include <cstdint>

namespace arch {

// Portable implementation of log2 for power-of-2 values
// Works on all platforms (MSVC, GCC, Clang)
static uint32_t log2_exact(uint32_t x) {
    uint32_t result = 0;
    while (x > 1) {
        x >>= 1;
        ++result;
    }
    return result;
}

Cache::Cache(const CacheConfig& cfg) : config_(cfg) {
    // Validate all cache configuration parameters
    assert(cfg.size_bytes > 0 && "Cache size must be positive");
    assert((cfg.size_bytes & (cfg.size_bytes - 1)) == 0 && "Cache size must be power of 2");
    assert((cfg.line_size & (cfg.line_size - 1)) == 0 && "Line size must be power of 2");
    assert((cfg.associativity & (cfg.associativity - 1)) == 0 && "Associativity must be power of 2");
    assert(cfg.size_bytes >= cfg.line_size * cfg.associativity && "Cache size must be >= line_size * associativity");
    
    uint32_t num_lines = cfg.size_bytes / cfg.line_size;
    uint32_t num_sets = num_lines / cfg.associativity;
    assert((num_sets & (num_sets - 1)) == 0 && "Number of sets must be power of 2");
    
    tags_.resize(num_lines, 0);
    valid_bits_.resize(num_lines, false);
    lru_counters_.resize(num_lines, 0);
}

uint32_t Cache::access(uint64_t address) {
    uint32_t offset_bits = log2_exact(config_.line_size);
    uint32_t num_sets = (config_.size_bytes / config_.line_size) / config_.associativity;
    uint32_t index_bits = log2_exact(num_sets);

    uint64_t index_mask = (1ULL << index_bits) - 1;
    uint64_t set_index  = (address >> offset_bits) & index_mask;
    uint64_t tag        = address >> (offset_bits + index_bits);

    uint32_t start = set_index * config_.associativity;
    uint32_t end   = start + config_.associativity;

    // Check for hit and update LRU on hit
    for (uint32_t i = start; i < end; ++i) {
        if (valid_bits_[i] && tags_[i] == tag) {
            ++hits_;
            lru_counters_[i] = ++global_lru_counter_;  // Update LRU timestamp
            return config_.hit_latency;
        }
    }

    // Miss: find victim using LRU policy
    ++misses_;
    uint32_t victim_idx = start;
    uint32_t min_lru = lru_counters_[start];
    
    // Find the least recently used way in this set
    for (uint32_t i = start; i < end; ++i) {
        if (!valid_bits_[i]) {
            // Prefer invalid lines (cold start)
            victim_idx = i;
            break;
        }
        if (lru_counters_[i] < min_lru) {
            min_lru = lru_counters_[i];
            victim_idx = i;
        }
    }
    
    // Install new line
    tags_[victim_idx] = tag;
    valid_bits_[victim_idx] = true;
    lru_counters_[victim_idx] = ++global_lru_counter_;
    
    return config_.miss_latency;
}

} // namespace arch