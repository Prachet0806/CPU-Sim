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

    uint32_t access(uint64_t address);
    uint32_t hit_latency()  const { return config_.hit_latency; }
    uint32_t miss_latency() const { return config_.miss_latency; }

private:
    CacheConfig config_;
    std::vector<uint64_t> tags_;
    std::vector<bool>     valid_bits_;
    uint64_t hits_   = 0;
    uint64_t misses_ = 0;
};

} // namespace arch