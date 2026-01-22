#include "arch/Cache.h"
#include <cassert>

namespace arch {

static uint32_t log2_exact(uint32_t x) {
    return static_cast<uint32_t>(__builtin_ctz(x));
}

Cache::Cache(const CacheConfig& cfg) : config_(cfg) {
    assert((cfg.line_size & (cfg.line_size - 1)) == 0);
    uint32_t num_lines = cfg.size_bytes / cfg.line_size;
    tags_.resize(num_lines, 0);
    valid_bits_.resize(num_lines, false);
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

    for (uint32_t i = start; i < end; ++i) {
        if (valid_bits_[i] && tags_[i] == tag) {
            ++hits_;
            return config_.hit_latency;
        }
    }

    ++misses_;
    tags_[start] = tag; // Simple replacement (Way 0)
    valid_bits_[start] = true;
    return config_.miss_latency;
}

} // namespace arch