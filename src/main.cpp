#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "arch/Pipeline.h"
#include "arch/Cache.h"
#include "arch/BranchPredictor.h"
#include "sim/Simulator.h"
#include "workload/MatMulWorkload.h"
#include "workload/Conv1DWorkload.h"

struct SimConfig {
    // Hardware config
    uint32_t l1_size = 32 * 1024;
    uint32_t l1_line_size = 64;
    uint32_t l1_associativity = 8;
    uint32_t l1_hit_latency = 4;
    uint32_t l1_miss_latency = 100;
    
    uint32_t pipeline_stages = 5;
    uint32_t pipeline_issue_width = 1;
    
    float bp_accuracy = 0.95f;
    uint32_t bp_mispredict_penalty = 15;
    
    // Workload config
    enum class WorkloadType { MATMUL, CONV1D } workload_type = WorkloadType::MATMUL;
    uint32_t matmul_n = 64;
    uint32_t conv1d_input_len = 1000;
    uint32_t conv1d_kernel_size = 3;
    
    // Output config
    bool json_output = false;
    bool show_progress = false;
    uint64_t progress_interval = 1000000;
    
    // Config file
    std::string config_file;
};

void print_usage(const char* prog_name) {
    std::cout << "CPU Microarchitecture Simulator\n";
    std::cout << "Usage: " << prog_name << " [options]\n\n";
    std::cout << "Hardware Options:\n";
    std::cout << "  --cache-size <bytes>        L1 cache size in bytes (default: 32768)\n";
    std::cout << "  --cache-line-size <bytes>   Cache line size in bytes (default: 64)\n";
    std::cout << "  --cache-assoc <ways>        Cache associativity (default: 8)\n";
    std::cout << "  --cache-hit-latency <cycles> Cache hit latency (default: 4)\n";
    std::cout << "  --cache-miss-latency <cycles> Cache miss latency (default: 100)\n";
    std::cout << "  --pipeline-stages <n>       Pipeline stages (default: 5)\n";
    std::cout << "  --issue-width <n>           Superscalar issue width (default: 1)\n";
    std::cout << "  --bp-accuracy <0-1>         Branch predictor accuracy (default: 0.95)\n";
    std::cout << "  --bp-penalty <cycles>       Branch mispredict penalty (default: 15)\n\n";
    std::cout << "Workload Options:\n";
    std::cout << "  --workload <matmul|conv1d>  Workload type (default: matmul)\n";
    std::cout << "  --matmul-n <n>              Matrix size for MatMul (default: 64)\n";
    std::cout << "  --conv1d-len <n>            Input length for Conv1D (default: 1000)\n";
    std::cout << "  --conv1d-kernel <n>         Kernel size for Conv1D (default: 3)\n\n";
    std::cout << "Output Options:\n";
    std::cout << "  --json                      Output results as JSON\n";
    std::cout << "  --progress                  Show progress during simulation\n";
    std::cout << "  --progress-interval <n>     Progress report interval (default: 1000000)\n\n";
    std::cout << "Config File:\n";
    std::cout << "  --config <file>             Load configuration from JSON file\n\n";
    std::cout << "Other:\n";
    std::cout << "  --help                      Show this help message\n";
    std::cout << "  --version                   Show version\n";
}

bool parse_uint(const std::string& str, uint32_t& out) {
    try {
        size_t pos;
        unsigned long val = std::stoul(str, &pos);
        if (pos != str.size()) return false;
        out = static_cast<uint32_t>(val);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_float(const std::string& str, float& out) {
    try {
        size_t pos;
        float val = std::stof(str, &pos);
        if (pos != str.size()) return false;
        out = val;
        return true;
    } catch (...) {
        return false;
    }
}

bool load_config_file(const std::string& filename, SimConfig& config) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << filename << "\n";
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Simple JSON parsing (for basic key-value pairs)
    // This is a minimal parser - for production use a proper JSON library
    auto extract_value = [&](const std::string& key, std::string& out) -> bool {
        std::string search = "\"" + key + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return false;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return false;
        pos = content.find_first_not_of(" \t\r\n", pos + 1);
        if (pos == std::string::npos) return false;
        
        char end_char = content[pos] == '"' ? '"' : ',';
        size_t start = (content[pos] == '"') ? pos + 1 : pos;
        size_t end = content.find(end_char, start);
        if (end == std::string::npos) return false;
        
        out = content.substr(start, end - start);
        return true;
    };
    
    std::string val;
    if (extract_value("cache_size", val)) parse_uint(val, config.l1_size);
    if (extract_value("cache_line_size", val)) parse_uint(val, config.l1_line_size);
    if (extract_value("cache_associativity", val)) parse_uint(val, config.l1_associativity);
    if (extract_value("cache_hit_latency", val)) parse_uint(val, config.l1_hit_latency);
    if (extract_value("cache_miss_latency", val)) parse_uint(val, config.l1_miss_latency);
    if (extract_value("pipeline_stages", val)) parse_uint(val, config.pipeline_stages);
    if (extract_value("issue_width", val)) parse_uint(val, config.pipeline_issue_width);
    if (extract_value("bp_accuracy", val)) parse_float(val, config.bp_accuracy);
    if (extract_value("bp_mispredict_penalty", val)) parse_uint(val, config.bp_mispredict_penalty);
    if (extract_value("workload_type", val)) {
        if (val == "conv1d") config.workload_type = SimConfig::WorkloadType::CONV1D;
    }
    if (extract_value("matmul_n", val)) parse_uint(val, config.matmul_n);
    if (extract_value("conv1d_input_len", val)) parse_uint(val, config.conv1d_input_len);
    if (extract_value("conv1d_kernel_size", val)) parse_uint(val, config.conv1d_kernel_size);
    if (extract_value("json_output", val)) config.json_output = (val == "true");
    if (extract_value("show_progress", val)) config.show_progress = (val == "true");
    if (extract_value("progress_interval", val)) {
        uint64_t interval;
        if (parse_uint(val, *reinterpret_cast<uint32_t*>(&interval))) config.progress_interval = interval;
    }
    
    return true;
}

void print_json_output(const SimConfig& config, const sim::SimStats& stats, double ipc) {
    std::cout << "{\n";
    std::cout << "  \"workload\": \"" << (config.workload_type == SimConfig::WorkloadType::MATMUL ? "matmul" : "conv1d") << "\",\n";
    std::cout << "  \"l1_cache_size\": " << config.l1_size << ",\n";
    std::cout << "  \"l1_line_size\": " << config.l1_line_size << ",\n";
    std::cout << "  \"l1_associativity\": " << config.l1_associativity << ",\n";
    std::cout << "  \"l1_hit_latency\": " << config.l1_hit_latency << ",\n";
    std::cout << "  \"l1_miss_latency\": " << config.l1_miss_latency << ",\n";
    std::cout << "  \"pipeline_stages\": " << config.pipeline_stages << ",\n";
    std::cout << "  \"pipeline_issue_width\": " << config.pipeline_issue_width << ",\n";
    std::cout << "  \"bp_accuracy\": " << config.bp_accuracy << ",\n";
    std::cout << "  \"bp_mispredict_penalty\": " << config.bp_mispredict_penalty << ",\n";
    std::cout << "  \"instructions_retired\": " << stats.instructions << ",\n";
    std::cout << "  \"total_cycles\": " << stats.cycles << ",\n";
    std::cout << "  \"ipc\": " << std::fixed << std::setprecision(2) << ipc << ",\n";
    std::cout << "  \"cache_misses\": " << stats.cache_misses << ",\n";
    std::cout << "  \"cache_hits\": " << stats.cache_hits << ",\n";
    std::cout << "  \"cache_accesses\": " << stats.cache_accesses << ",\n";
    std::cout << "  \"branch_mispredicts\": " << stats.branch_mispredicts << ",\n";
    std::cout << "  \"branch_predictions\": " << stats.branch_predictions << "\n";
    std::cout << "}\n";
}

void progress_callback(uint64_t instructions, uint64_t cycles) {
    std::cerr << "[PROGRESS] Instructions: " << instructions << ", Cycles: " << cycles << "\n";
}

int main(int argc, char* argv[]) {
    SimConfig config;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            std::cout << "cpu-sim version 1.0.0\n";
            return 0;
        } else if (arg == "--config") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --config requires a file argument\n";
                return 1;
            }
            config.config_file = argv[++i];
            if (!load_config_file(config.config_file, config)) {
                return 1;
            }
        } else if (arg == "--cache-size") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_size)) {
                std::cerr << "Error: --cache-size requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--cache-line-size") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_line_size)) {
                std::cerr << "Error: --cache-line-size requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--cache-assoc") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_associativity)) {
                std::cerr << "Error: --cache-assoc requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--cache-hit-latency") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_hit_latency)) {
                std::cerr << "Error: --cache-hit-latency requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--cache-miss-latency") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_miss_latency)) {
                std::cerr << "Error: --cache-miss-latency requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--pipeline-stages") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.pipeline_stages)) {
                std::cerr << "Error: --pipeline-stages requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--issue-width") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.pipeline_issue_width)) {
                std::cerr << "Error: --issue-width requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--bp-accuracy") {
            if (i + 1 >= argc || !parse_float(argv[++i], config.bp_accuracy)) {
                std::cerr << "Error: --bp-accuracy requires a float between 0 and 1\n";
                return 1;
            }
        } else if (arg == "--bp-penalty") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.bp_mispredict_penalty)) {
                std::cerr << "Error: --bp-penalty requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--workload") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --workload requires matmul or conv1d\n";
                return 1;
            }
            std::string wl = argv[++i];
            if (wl == "matmul") config.workload_type = SimConfig::WorkloadType::MATMUL;
            else if (wl == "conv1d") config.workload_type = SimConfig::WorkloadType::CONV1D;
            else {
                std::cerr << "Error: Unknown workload type: " << wl << "\n";
                return 1;
            }
        } else if (arg == "--matmul-n") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.matmul_n)) {
                std::cerr << "Error: --matmul-n requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--conv1d-len") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.conv1d_input_len)) {
                std::cerr << "Error: --conv1d-len requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--conv1d-kernel") {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.conv1d_kernel_size)) {
                std::cerr << "Error: --conv1d-kernel requires a positive integer\n";
                return 1;
            }
        } else if (arg == "--json") {
            config.json_output = true;
        } else if (arg == "--progress") {
            config.show_progress = true;
        } else if (arg == "--progress-interval") {
            if (i + 1 >= argc || !parse_uint(argv[++i], *reinterpret_cast<uint32_t*>(&config.progress_interval))) {
                std::cerr << "Error: --progress-interval requires a positive integer\n";
                return 1;
            }
        } else if (arg[0] != '-') {
            // Legacy positional argument: cache size
            if (!parse_uint(arg, config.l1_size)) {
                std::cerr << "Error: Invalid cache size: " << arg << "\n";
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate config
    if (config.l1_size == 0 || (config.l1_size & (config.l1_size - 1)) != 0) {
        std::cerr << "Error: Cache size must be a power of 2\n";
        return 1;
    }
    if (config.l1_line_size == 0 || (config.l1_line_size & (config.l1_line_size - 1)) != 0) {
        std::cerr << "Error: Cache line size must be a power of 2\n";
        return 1;
    }
    if (config.l1_associativity == 0 || (config.l1_associativity & (config.l1_associativity - 1)) != 0) {
        std::cerr << "Error: Cache associativity must be a power of 2\n";
        return 1;
    }
    if (config.l1_size < config.l1_line_size * config.l1_associativity) {
        std::cerr << "Error: Cache size must be >= line_size * associativity\n";
        return 1;
    }
    if (config.l1_hit_latency == 0) {
        std::cerr << "Error: Cache hit latency must be positive\n";
        return 1;
    }
    if (config.l1_miss_latency <= config.l1_hit_latency) {
        std::cerr << "Error: Cache miss latency must be greater than hit latency\n";
        return 1;
    }
    if (config.pipeline_stages == 0) {
        std::cerr << "Error: Pipeline stages must be positive\n";
        return 1;
    }
    if (config.pipeline_issue_width == 0) {
        std::cerr << "Error: Issue width must be positive\n";
        return 1;
    }
    if (config.bp_accuracy < 0.0f || config.bp_accuracy > 1.0f) {
        std::cerr << "Error: Branch predictor accuracy must be between 0 and 1\n";
        return 1;
    }
    
    // Create hardware components
    arch::PipelineConfig pipe_cfg{config.pipeline_stages, config.pipeline_issue_width};
    arch::CacheConfig cache_cfg{config.l1_size, config.l1_line_size, config.l1_associativity, 
                                 config.l1_hit_latency, config.l1_miss_latency};
    arch::BranchPredictorConfig bp_cfg{config.bp_accuracy, config.bp_mispredict_penalty};
    
    arch::Pipeline pipeline(pipe_cfg);
    arch::Cache cache(cache_cfg);
    arch::BranchPredictor bp(bp_cfg);
    
    // Create workload
    workload::MatMulWorkload matmul_workload(config.matmul_n);
    workload::Conv1DWorkload conv1d_workload(config.conv1d_input_len, config.conv1d_kernel_size);
    workload::Workload& workload = (config.workload_type == SimConfig::WorkloadType::MATMUL) 
        ? static_cast<workload::Workload&>(matmul_workload)
        : static_cast<workload::Workload&>(conv1d_workload);
    
    // Run simulation
    sim::Simulator simulator(pipeline, cache, bp);
    
    if (config.show_progress) {
        simulator.set_progress_callback(progress_callback, config.progress_interval);
    }
    
    auto stats = simulator.run(workload);
    
    // Report results
    double ipc = stats.cycles > 0
        ? static_cast<double>(stats.instructions) / static_cast<double>(stats.cycles)
        : 0.0;
    
    if (config.json_output) {
        print_json_output(config, stats, ipc);
    } else {
        std::cout << "\n[SIMULATION COMPLETE]\n";
        std::cout << "Workload:             " << (config.workload_type == SimConfig::WorkloadType::MATMUL ? "Matrix Multiplication" : "Conv1D") << "\n";
        if (config.workload_type == SimConfig::WorkloadType::MATMUL) {
            std::cout << "Matrix Size (N):      " << config.matmul_n << "\n";
        } else {
            std::cout << "Input Length:         " << config.conv1d_input_len << "\n";
            std::cout << "Kernel Size:          " << config.conv1d_kernel_size << "\n";
        }
        std::cout << "L1 Cache Size:        " << config.l1_size << " Bytes\n";
        std::cout << "L1 Line Size:         " << config.l1_line_size << " Bytes\n";
        std::cout << "L1 Associativity:     " << config.l1_associativity << "-way\n";
        std::cout << "Pipeline Stages:      " << config.pipeline_stages << "\n";
        std::cout << "Issue Width:          " << config.pipeline_issue_width << "\n";
        std::cout << "BP Accuracy:          " << config.bp_accuracy << "\n";
        std::cout << "BP Mispredict Penalty:" << config.bp_mispredict_penalty << " cycles\n";
        std::cout << "--------------------------------\n";
        std::cout << "Instructions Retired: " << stats.instructions << "\n";
        std::cout << "Total Cycles:         " << stats.cycles << "\n";
        std::cout << "IPC:                  " << std::fixed << std::setprecision(2) << ipc << "\n";
        std::cout << "--------------------------------\n";
        std::cout << "Cache Accesses:       " << stats.cache_accesses << "\n";
        std::cout << "Cache Hits:           " << stats.cache_hits << "\n";
        std::cout << "Cache Misses:         " << stats.cache_misses << "\n";
        std::cout << "Branch Predictions:   " << stats.branch_predictions << "\n";
        std::cout << "Branch Mispredicts:   " << stats.branch_mispredicts << "\n";
        std::cout << "--------------------------------\n";
    }
    
    return 0;
}