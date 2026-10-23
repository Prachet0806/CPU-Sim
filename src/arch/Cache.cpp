// src/arch/Cache.cpp
#include "arch/Cache.h"
#include <cstdint>
#include <stdexcept>

namespace arch
{

// Portable implementation of log2 for power-of-2 values
// Works on all platforms (MSVC, GCC, Clang)
static uint32_t log2_exact(uint32_t x)
{
    uint32_t result = 0;
    while (x > 1)
    {
        x >>= 1;
        ++result;
    }
    return result;
}

Cache::Cache(const CacheConfig& cfg) : config_(cfg)
{
    const auto power_of_two = [](uint32_t value) { return value != 0 && (value & (value - 1)) == 0; };
    if (!power_of_two(cfg.size_bytes) || !power_of_two(cfg.line_size) || !power_of_two(cfg.associativity) ||
        static_cast<uint64_t>(cfg.size_bytes) < static_cast<uint64_t>(cfg.line_size) * cfg.associativity ||
        cfg.hit_latency == 0 || cfg.miss_latency <= cfg.hit_latency)
    {
        throw std::invalid_argument("Invalid cache configuration");
    }

    uint32_t num_lines = cfg.size_bytes / cfg.line_size;
    uint32_t num_sets = num_lines / cfg.associativity;
    if (!power_of_two(num_sets))
    {
        throw std::invalid_argument("Cache set count must be a power of two");
    }

    tags_.resize(num_lines, 0);
    valid_bits_.resize(num_lines, false);
    lru_counters_.resize(num_lines, 0);
}

uint32_t Cache::access(uint64_t address)
{
    uint32_t offset_bits = log2_exact(config_.line_size);
    uint32_t num_sets = (config_.size_bytes / config_.line_size) / config_.associativity;
    uint32_t index_bits = log2_exact(num_sets);

    uint64_t index_mask = (index_bits >= 64) ? ~0ULL : ((1ULL << index_bits) - 1);
    uint64_t set_index = (address >> offset_bits) & index_mask;
    uint64_t tag = address >> (offset_bits + index_bits);

    uint32_t start = static_cast<uint32_t>(set_index * config_.associativity);
    uint32_t end = start + config_.associativity;

    // Check for hit and update LRU on hit
    for (uint32_t i = start; i < end; ++i)
    {
        if (valid_bits_[i] && tags_[i] == tag)
        {
            ++hits_;
            lru_counters_[i] = ++global_lru_counter_;
            return config_.hit_latency;
        }
    }

    // Miss: find victim using LRU policy
    ++misses_;
    uint32_t victim_idx = start;
    uint64_t min_lru = lru_counters_[start];

    // Find the least recently used way in this set
    for (uint32_t i = start; i < end; ++i)
    {
        if (!valid_bits_[i])
        {
            // Prefer invalid lines (cold start)
            victim_idx = i;
            break;
        }
        if (lru_counters_[i] < min_lru)
        {
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
