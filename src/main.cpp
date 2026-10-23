#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>

#include "arch/Pipeline.h"
#include "arch/Cache.h"
#include "arch/BranchPredictor.h"
#include "sim/Simulator.h"
#include "workload/MatMulWorkload.h"
#include "workload/Conv1DWorkload.h"
#include "workload/BranchWorkload.h"

struct SimConfig
{
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
    enum class WorkloadType
    {
        MATMUL,
        CONV1D,
        BRANCH
    } workload_type = WorkloadType::MATMUL;
    uint32_t matmul_n = 64;
    uint32_t conv1d_input_len = 1000;
    uint32_t conv1d_kernel_size = 3;
    uint32_t branch_count = 1000;

    // Output config
    bool json_output = false;
    bool show_progress = false;
    uint64_t progress_interval = 1000000;

    // Config file
    std::string config_file;
};

void print_usage(const char* prog_name)
{
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
    std::cout << "  --workload <matmul|conv1d|branch> Workload type (default: matmul)\n";
    std::cout << "  --matmul-n <n>              Matrix size for MatMul (default: 64)\n";
    std::cout << "  --conv1d-len <n>            Input length for Conv1D (default: 1000)\n";
    std::cout << "  --conv1d-kernel <n>         Kernel size for Conv1D (default: 3)\n\n";
    std::cout << "  --branch-count <n>          Branches in branch workload (default: 1000)\n";
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

bool parse_uint(const std::string& str, uint32_t& out)
{
    if (str.empty())
        return false;
    // Reject negative-looking input (stoul would wrap to a huge unsigned)
    if (str[0] == '-')
        return false;
    try
    {
        size_t pos;
        unsigned long long val = std::stoull(str, &pos);
        if (pos != str.size() || val > std::numeric_limits<uint32_t>::max())
            return false;
        out = static_cast<uint32_t>(val);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool parse_float(const std::string& str, float& out)
{
    try
    {
        size_t pos;
        float val = std::stof(str, &pos);
        if (pos != str.size() || !std::isfinite(val))
            return false;
        out = val;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

struct JsonScalar
{
    enum class Kind
    {
        STRING,
        NUMBER,
        BOOLEAN
    } kind;
    std::string value;
};

// The config schema is a flat JSON object of strings, numbers, and booleans.
class ConfigJsonParser
{
  public:
    explicit ConfigJsonParser(const std::string& source) : source_(source) {}

    bool parse(std::map<std::string, JsonScalar>& values)
    {
        skip_space();
        if (!take('{'))
            return false;
        skip_space();
        if (take('}'))
            return at_end();
        do
        {
            std::string key;
            if (!parse_string(key) || !take(':'))
                return false;
            JsonScalar scalar{};
            skip_space();
            if (peek() == '"')
            {
                scalar.kind = JsonScalar::Kind::STRING;
                if (!parse_string(scalar.value))
                    return false;
            }
            else if (source_.compare(position_, 4, "true") == 0 || source_.compare(position_, 5, "false") == 0)
            {
                scalar.kind = JsonScalar::Kind::BOOLEAN;
                const bool truth = source_.compare(position_, 4, "true") == 0;
                scalar.value = truth ? "true" : "false";
                position_ += truth ? 4 : 5;
            }
            else
            {
                scalar.kind = JsonScalar::Kind::NUMBER;
                if (!parse_number(scalar.value))
                    return false;
            }
            if (!values.emplace(key, scalar).second)
                return false;
            skip_space();
            if (take('}'))
                return at_end();
        } while (take(','));
        return false;
    }

  private:
    const std::string& source_;
    size_t position_ = 0;

    void skip_space()
    {
        while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_])) != 0)
            ++position_;
    }
    char peek() const { return position_ < source_.size() ? source_[position_] : '\0'; }
    bool take(char expected)
    {
        skip_space();
        if (peek() != expected)
            return false;
        ++position_;
        return true;
    }
    bool at_end()
    {
        skip_space();
        return position_ == source_.size();
    }

    static void append_utf8(std::string& out, uint32_t codepoint)
    {
        if (codepoint <= 0x7f)
            out.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7ff)
        {
            out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
        else if (codepoint <= 0xffff)
        {
            out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
        else
        {
            out.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    bool hex4(uint32_t& value)
    {
        if (source_.size() - position_ < 4)
            return false;
        value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char digit = source_[position_++];
            value <<= 4;
            if (digit >= '0' && digit <= '9')
                value += static_cast<uint32_t>(digit - '0');
            else if (digit >= 'a' && digit <= 'f')
                value += static_cast<uint32_t>(digit - 'a' + 10);
            else if (digit >= 'A' && digit <= 'F')
                value += static_cast<uint32_t>(digit - 'A' + 10);
            else
                return false;
        }
        return true;
    }

    bool parse_string(std::string& out)
    {
        if (!take('"'))
            return false;
        while (position_ < source_.size())
        {
            const unsigned char ch = static_cast<unsigned char>(source_[position_++]);
            if (ch == '"')
                return true;
            if (ch < 0x20)
                return false;
            if (ch != '\\')
            {
                out.push_back(static_cast<char>(ch));
                continue;
            }
            if (position_ == source_.size())
                return false;
            const char escaped = source_[position_++];
            switch (escaped)
            {
            case '"':
            case '\\':
            case '/':
                out.push_back(escaped);
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'u':
            {
                uint32_t codepoint = 0;
                if (!hex4(codepoint))
                    return false;
                if (codepoint >= 0xd800 && codepoint <= 0xdbff)
                {
                    if (position_ + 2 > source_.size() || source_[position_++] != '\\' || source_[position_++] != 'u')
                        return false;
                    uint32_t low = 0;
                    if (!hex4(low) || low < 0xdc00 || low > 0xdfff)
                        return false;
                    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
                }
                else if (codepoint >= 0xdc00 && codepoint <= 0xdfff)
                    return false;
                append_utf8(out, codepoint);
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool parse_number(std::string& out)
    {
        const size_t start = position_;
        if (peek() == '-')
            ++position_;
        if (peek() == '0')
            ++position_;
        else
        {
            if (peek() < '1' || peek() > '9')
                return false;
            do
            {
                ++position_;
            } while (peek() >= '0' && peek() <= '9');
        }
        if (peek() == '.')
        {
            ++position_;
            if (peek() < '0' || peek() > '9')
                return false;
            do
            {
                ++position_;
            } while (peek() >= '0' && peek() <= '9');
        }
        if (peek() == 'e' || peek() == 'E')
        {
            ++position_;
            if (peek() == '+' || peek() == '-')
                ++position_;
            if (peek() < '0' || peek() > '9')
                return false;
            do
            {
                ++position_;
            } while (peek() >= '0' && peek() <= '9');
        }
        out = source_.substr(start, position_ - start);
        return true;
    }
};

bool load_config_file(const std::string& filename, SimConfig& config)
{
    std::ifstream file(filename);
    if (!file)
    {
        std::cerr << "Error: Cannot open config file: " << filename << "\n";
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::map<std::string, JsonScalar> values;
    const std::string content = buffer.str();
    ConfigJsonParser parser(content);
    if (!parser.parse(values))
    {
        std::cerr << "Error: Invalid flat JSON configuration\n";
        return false;
    }
    const std::pair<const char*, const char*> aliases[] = {{"cache_size", "l1_cache_size"},
                                                           {"cache_line_size", "l1_line_size"},
                                                           {"cache_associativity", "l1_associativity"},
                                                           {"cache_hit_latency", "l1_hit_latency"},
                                                           {"cache_miss_latency", "l1_miss_latency"},
                                                           {"issue_width", "pipeline_issue_width"},
                                                           {"workload", "workload_type"}};
    for (const auto& alias : aliases)
    {
        if (values.count(alias.first) != 0 && values.count(alias.second) != 0)
        {
            std::cerr << "Error: Duplicate config field aliases: " << alias.first << "\n";
            return false;
        }
    }
    for (const auto& item : values)
    {
        const auto& key = item.first;
        const auto& value = item.second;
        const bool number = value.kind == JsonScalar::Kind::NUMBER;
        const bool boolean = value.kind == JsonScalar::Kind::BOOLEAN;
        const bool string = value.kind == JsonScalar::Kind::STRING;
        bool valid = true;
        if (key == "cache_size" || key == "l1_cache_size")
            valid = number && parse_uint(value.value, config.l1_size);
        else if (key == "cache_line_size" || key == "l1_line_size")
            valid = number && parse_uint(value.value, config.l1_line_size);
        else if (key == "cache_associativity" || key == "l1_associativity")
            valid = number && parse_uint(value.value, config.l1_associativity);
        else if (key == "cache_hit_latency" || key == "l1_hit_latency")
            valid = number && parse_uint(value.value, config.l1_hit_latency);
        else if (key == "cache_miss_latency" || key == "l1_miss_latency")
            valid = number && parse_uint(value.value, config.l1_miss_latency);
        else if (key == "pipeline_stages")
            valid = number && parse_uint(value.value, config.pipeline_stages);
        else if (key == "issue_width" || key == "pipeline_issue_width")
            valid = number && parse_uint(value.value, config.pipeline_issue_width);
        else if (key == "bp_accuracy")
            valid = number && parse_float(value.value, config.bp_accuracy);
        else if (key == "bp_mispredict_penalty")
            valid = number && parse_uint(value.value, config.bp_mispredict_penalty);
        else if (key == "matmul_n")
            valid = number && parse_uint(value.value, config.matmul_n);
        else if (key == "conv1d_input_len")
            valid = number && parse_uint(value.value, config.conv1d_input_len);
        else if (key == "conv1d_kernel_size")
            valid = number && parse_uint(value.value, config.conv1d_kernel_size);
        else if (key == "branch_count")
            valid = number && parse_uint(value.value, config.branch_count);
        else if (key == "progress_interval")
        {
            uint32_t interval = 0;
            valid = number && parse_uint(value.value, interval);
            if (valid)
                config.progress_interval = interval;
        }
        else if (key == "json_output")
        {
            valid = boolean;
            if (valid)
                config.json_output = value.value == "true";
        }
        else if (key == "show_progress")
        {
            valid = boolean;
            if (valid)
                config.show_progress = value.value == "true";
        }
        else if (key == "workload" || key == "workload_type")
        {
            valid = string;
            if (value.value == "matmul")
                config.workload_type = SimConfig::WorkloadType::MATMUL;
            else if (value.value == "conv1d")
                config.workload_type = SimConfig::WorkloadType::CONV1D;
            else if (value.value == "branch")
                config.workload_type = SimConfig::WorkloadType::BRANCH;
            else
                valid = false;
        }
        else
            valid = false;
        if (!valid)
        {
            std::cerr << "Error: Invalid or unknown config field: " << key << "\n";
            return false;
        }
    }
    return true;
}

void print_json_output(const SimConfig& config, const sim::SimStats& stats, double ipc)
{
    std::cout << "{\n";
    std::cout << "  \"workload\": \""
              << (config.workload_type == SimConfig::WorkloadType::MATMUL   ? "matmul"
                  : config.workload_type == SimConfig::WorkloadType::CONV1D ? "conv1d"
                                                                            : "branch")
              << "\",\n";
    if (config.workload_type == SimConfig::WorkloadType::BRANCH)
    {
        std::cout << "  \"branch_count\": " << config.branch_count << ",\n";
    }
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

void progress_callback(uint64_t instructions, uint64_t cycles)
{
    std::cerr << "[PROGRESS] Instructions: " << instructions << ", Cycles: " << cycles << "\n";
}

int main(int argc, char* argv[])
{
    SimConfig config;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--version")
        {
            std::cout << "cpu-sim version 1.0.0\n";
            return 0;
        }
        else if (arg == "--config")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --config requires a file argument\n";
                return 1;
            }
            config.config_file = argv[++i];
            if (!load_config_file(config.config_file, config))
            {
                return 1;
            }
        }
        else if (arg == "--cache-size")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_size))
            {
                std::cerr << "Error: --cache-size requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--cache-line-size")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_line_size))
            {
                std::cerr << "Error: --cache-line-size requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--cache-assoc")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_associativity))
            {
                std::cerr << "Error: --cache-assoc requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--cache-hit-latency")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_hit_latency))
            {
                std::cerr << "Error: --cache-hit-latency requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--cache-miss-latency")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.l1_miss_latency))
            {
                std::cerr << "Error: --cache-miss-latency requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--pipeline-stages")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.pipeline_stages))
            {
                std::cerr << "Error: --pipeline-stages requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--issue-width")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.pipeline_issue_width))
            {
                std::cerr << "Error: --issue-width requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--bp-accuracy")
        {
            if (i + 1 >= argc || !parse_float(argv[++i], config.bp_accuracy))
            {
                std::cerr << "Error: --bp-accuracy requires a float between 0 and 1\n";
                return 1;
            }
        }
        else if (arg == "--bp-penalty")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.bp_mispredict_penalty))
            {
                std::cerr << "Error: --bp-penalty requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--workload")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --workload requires matmul, conv1d, or branch\n";
                return 1;
            }
            std::string wl = argv[++i];
            if (wl == "matmul")
                config.workload_type = SimConfig::WorkloadType::MATMUL;
            else if (wl == "conv1d")
                config.workload_type = SimConfig::WorkloadType::CONV1D;
            else if (wl == "branch")
                config.workload_type = SimConfig::WorkloadType::BRANCH;
            else
            {
                std::cerr << "Error: Unknown workload type: " << wl << "\n";
                return 1;
            }
        }
        else if (arg == "--matmul-n")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.matmul_n))
            {
                std::cerr << "Error: --matmul-n requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--conv1d-len")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.conv1d_input_len))
            {
                std::cerr << "Error: --conv1d-len requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--conv1d-kernel")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.conv1d_kernel_size))
            {
                std::cerr << "Error: --conv1d-kernel requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--branch-count")
        {
            if (i + 1 >= argc || !parse_uint(argv[++i], config.branch_count))
            {
                std::cerr << "Error: --branch-count requires a positive integer\n";
                return 1;
            }
        }
        else if (arg == "--json")
        {
            config.json_output = true;
        }
        else if (arg == "--progress")
        {
            config.show_progress = true;
        }
        else if (arg == "--progress-interval")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --progress-interval requires a positive integer\n";
                return 1;
            }
            uint32_t parsed_interval;
            if (!parse_uint(argv[++i], parsed_interval))
            {
                std::cerr << "Error: --progress-interval requires a positive integer\n";
                return 1;
            }
            config.progress_interval = parsed_interval;
        }
        else if (arg[0] != '-')
        {
            // Legacy positional argument: cache size
            if (!parse_uint(arg, config.l1_size))
            {
                std::cerr << "Error: Invalid cache size: " << arg << "\n";
                return 1;
            }
        }
        else
        {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate config
    if (config.l1_size == 0 || (config.l1_size & (config.l1_size - 1)) != 0)
    {
        std::cerr << "Error: Cache size must be a power of 2\n";
        return 1;
    }
    if (config.l1_line_size == 0 || (config.l1_line_size & (config.l1_line_size - 1)) != 0)
    {
        std::cerr << "Error: Cache line size must be a power of 2\n";
        return 1;
    }
    if (config.l1_associativity == 0 || (config.l1_associativity & (config.l1_associativity - 1)) != 0)
    {
        std::cerr << "Error: Cache associativity must be a power of 2\n";
        return 1;
    }
    if (static_cast<uint64_t>(config.l1_size) < static_cast<uint64_t>(config.l1_line_size) * config.l1_associativity)
    {
        std::cerr << "Error: Cache size must be >= line_size * associativity\n";
        return 1;
    }
    if (config.l1_hit_latency == 0)
    {
        std::cerr << "Error: Cache hit latency must be positive\n";
        return 1;
    }
    if (config.l1_miss_latency <= config.l1_hit_latency)
    {
        std::cerr << "Error: Cache miss latency must be greater than hit latency\n";
        return 1;
    }
    if (config.pipeline_stages != 5)
    {
        std::cerr << "Error: Pipeline stages must be exactly 5 (F, D, E, M, W)\n";
        return 1;
    }
    if (config.pipeline_issue_width == 0 || config.pipeline_issue_width > 8)
    {
        std::cerr << "Error: Issue width must be between 1 and 8\n";
        return 1;
    }
    if (!std::isfinite(config.bp_accuracy) || config.bp_accuracy < 0.0f || config.bp_accuracy > 1.0f)
    {
        std::cerr << "Error: Branch predictor accuracy must be between 0 and 1\n";
        return 1;
    }
    if (config.matmul_n == 0 || config.matmul_n > 8192)
    {
        std::cerr << "Error: matmul-n must be between 1 and 8192\n";
        return 1;
    }
    if (config.conv1d_input_len == 0 || config.conv1d_input_len > 67108864)
    {
        std::cerr << "Error: conv1d-len must be between 1 and 67108864\n";
        return 1;
    }
    if (config.conv1d_kernel_size == 0)
    {
        std::cerr << "Error: conv1d-kernel must be >= 1\n";
        return 1;
    }
    if (config.conv1d_kernel_size > config.conv1d_input_len)
    {
        std::cerr << "Error: conv1d-kernel must be <= conv1d-len\n";
        return 1;
    }
    if (config.progress_interval == 0)
    {
        std::cerr << "Error: progress-interval must be positive\n";
        return 1;
    }
    if (config.workload_type == SimConfig::WorkloadType::BRANCH && config.branch_count == 0)
    {
        std::cerr << "Error: branch-count must be >= 1\n";
        return 1;
    }

    // Create hardware components
    arch::PipelineConfig pipe_cfg{config.pipeline_stages, config.pipeline_issue_width};
    arch::CacheConfig cache_cfg{config.l1_size, config.l1_line_size, config.l1_associativity, config.l1_hit_latency,
                                config.l1_miss_latency};
    arch::BranchPredictorConfig bp_cfg{config.bp_accuracy, config.bp_mispredict_penalty};

    arch::Pipeline pipeline(pipe_cfg);
    arch::Cache cache(cache_cfg);
    arch::BranchPredictor bp(bp_cfg);

    // Create workload
    workload::MatMulWorkload matmul_workload(config.matmul_n);
    workload::Conv1DWorkload conv1d_workload(config.conv1d_input_len, config.conv1d_kernel_size);
    workload::BranchWorkload branch_workload(config.branch_count);
    workload::Workload& workload =
        (config.workload_type == SimConfig::WorkloadType::MATMUL)   ? static_cast<workload::Workload&>(matmul_workload)
        : (config.workload_type == SimConfig::WorkloadType::CONV1D) ? static_cast<workload::Workload&>(conv1d_workload)
                                                                    : static_cast<workload::Workload&>(branch_workload);

    // Run simulation
    sim::Simulator simulator(pipeline, cache, bp);

    if (config.show_progress)
    {
        simulator.set_progress_callback(progress_callback, config.progress_interval);
    }

    auto stats = simulator.run(workload);

    // Report results
    double ipc = stats.cycles > 0 ? static_cast<double>(stats.instructions) / static_cast<double>(stats.cycles) : 0.0;

    if (config.json_output)
    {
        print_json_output(config, stats, ipc);
    }
    else
    {
        std::cout << "\n[SIMULATION COMPLETE]\n";
        std::cout << "Workload:             "
                  << (config.workload_type == SimConfig::WorkloadType::MATMUL   ? "Matrix Multiplication"
                      : config.workload_type == SimConfig::WorkloadType::CONV1D ? "Conv1D"
                                                                                : "Branch")
                  << "\n";
        if (config.workload_type == SimConfig::WorkloadType::MATMUL)
        {
            std::cout << "Matrix Size (N):      " << config.matmul_n << "\n";
        }
        else if (config.workload_type == SimConfig::WorkloadType::CONV1D)
        {
            std::cout << "Input Length:         " << config.conv1d_input_len << "\n";
            std::cout << "Kernel Size:          " << config.conv1d_kernel_size << "\n";
        }
        else
        {
            std::cout << "Branch Count:         " << config.branch_count << "\n";
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
