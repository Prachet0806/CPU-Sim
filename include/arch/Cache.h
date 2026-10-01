//include/arch/Cache.h
#pragma once
#include <cstdint>
#include <vector>

namespace arch {

/**
 * @brief Configuration parameters for the cache.
 */
struct CacheConfig {
    uint32_t size_bytes;     ///< Total cache size in bytes (must be power of 2)
    uint32_t line_size;      ///< Cache line size in bytes (must be power of 2)
    uint32_t associativity;  ///< Associativity (number of ways, must be power of 2)
    uint32_t hit_latency;    ///< Hit latency in cycles (must be > 0)
    uint32_t miss_latency;   ///< Miss latency in cycles (must be > hit_latency)
};

/**
 * @brief Set-associative cache simulator with LRU replacement.
 * 
 * Models a blocking L1 cache with configurable size, line size, and associativity.
 * Uses bitwise address decoding (tag/index/offset) and LRU replacement policy.
 * Provides deterministic behavior for architectural exploration.
 */
class Cache {
public:
    /**
     * @brief Construct cache with given configuration.
     * @param cfg Cache configuration
     * @throws std::invalid_argument if configuration is invalid (via assert)
     */
    explicit Cache(const CacheConfig& cfg);
    
    // Prevent copying and moving (cache state should not be duplicated)
    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;
    Cache(Cache&&) = delete;
    Cache& operator=(Cache&&) = delete;
    
    /**
     * @brief Access cache at given address.
     * @param address Memory address to access
     * @return Latency in cycles (hit_latency on hit, miss_latency on miss)
     */
    uint32_t access(uint64_t address);
    
    uint32_t hit_latency()  const { return config_.hit_latency; }
    uint32_t miss_latency() const { return config_.miss_latency; }
    
    // Expose cache statistics
    uint64_t get_hits() const { return hits_; }
    uint64_t get_misses() const { return misses_; }
    uint64_t get_accesses() const { return hits_ + misses_; }

private:
    CacheConfig config_;
    std::vector<uint64_t> tags_;           ///< Tag array (one per cache line)
    std::vector<bool>     valid_bits_;     ///< Valid bit array
    std::vector<uint32_t> lru_counters_;   ///< LRU timestamp per line
    uint64_t hits_   = 0;
    uint64_t misses_ = 0;
    uint32_t global_lru_counter_ = 0;      ///< Global timestamp for LRU
};

} // namespace arch