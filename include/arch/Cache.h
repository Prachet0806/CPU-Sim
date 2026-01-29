#pragma once
#include <cstdint>
#include <vector>

namespace arch {

struct CacheConfig {
    uint32_t size_bytes;
    uint32_t line_size;
    uint32_t associativity;
    uint32_t hit_latency;
    uint32_t miss_latency;
};

class Cache {
public:
    explicit Cache(const CacheConfig& cfg);

    // Prevent copying and moving (cache state should not be duplicated)
    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;
    Cache(Cache&&) = delete;
    Cache& operator=(Cache&&) = delete;

    uint32_t access(uint64_t address);
    uint32_t hit_latency()  const { return config_.hit_latency; }
    uint32_t miss_latency() const { return config_.miss_latency; }
    
    // Expose cache statistics
    uint64_t get_hits() const { return hits_; }
    uint64_t get_misses() const { return misses_; }
    uint64_t get_accesses() const { return hits_ + misses_; }

private:
    CacheConfig config_;
    std::vector<uint64_t> tags_;
    std::vector<bool>     valid_bits_;
    std::vector<uint32_t> lru_counters_;  // LRU replacement tracking
    uint64_t hits_   = 0;
    uint64_t misses_ = 0;
    uint32_t global_lru_counter_ = 0;  // Global timestamp for LRU
};

} // namespace arch