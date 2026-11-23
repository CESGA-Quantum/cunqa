#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <string>

#include "args_qraise.hpp"

#include "utils/registry.hpp"
#include "logger.hpp"

enum class Communications { NC, CC, QC };

bool exists_family_name(const std::string& family)
{
    return !cunqa::read_qpus(family).empty();
}

// The family name is used as the name of its registry directory
bool valid_family_name(const std::string& family)
{
    return !family.empty() && family[0] != '.' && family.find('/') == std::string::npos;
}

bool communication_supported_by_simulator(const std::string& simulator, const Communications& scheme) 
{
    switch (scheme) {
        case Communications::NC:
            return std::ranges::find(cunqa::SUPPORTED_SIMPLE_SIMULATORS, simulator)
                != cunqa::SUPPORTED_SIMPLE_SIMULATORS.end();
        case Communications::CC:
            return std::ranges::find(cunqa::SUPPORTED_CC_SIMULATORS, simulator)
                != cunqa::SUPPORTED_CC_SIMULATORS.end();
        case Communications::QC:
            return std::ranges::find(cunqa::SUPPORTED_QC_SIMULATORS, simulator)
                != cunqa::SUPPORTED_QC_SIMULATORS.end();
    }

    return false;
}

std::string get_communications_name(const Communications& scheme)
{
    switch (scheme) {
        case Communications::NC:
            return "nc";
        case Communications::CC:
            return "cc";
        case Communications::QC:
            return "qc";
    }

    return std::string();
};

Communications get_communications_class(const std::string_view& scheme_name)
{
    if (scheme_name == "nc")
        return Communications::NC;
    if (scheme_name == "cc")
        return Communications::CC;
    if (scheme_name == "qc")
        return Communications::QC;
    return Communications::NC;
};


void remove_tmp_files(const std::string filepath = "")
{
    if (!filepath.empty()) {
        std::string rmv_cmd = "rm " + filepath;
        std::system(rmv_cmd.c_str());
    } else {
        std::system("rm qraise_sbatch_tmp.sbatch");
    }
}


// Memory is handled in MB to avoid the truncations of whole-GB integer arithmetic.
constexpr std::int64_t MB_PER_GB = 1024;
// A vQPU process needs at least this much memory.
constexpr std::int64_t MIN_MEM_PER_QPU_MB = 256;
// quantum_comm: memory of each vQPU server, taken out of --mem-per-qpu. The remainder goes
// to the shared simulator process (executor).
constexpr std::int64_t MIN_SERVER_MEM_MB = 128;
constexpr std::int64_t MAX_SERVER_MEM_MB = 1 * MB_PER_GB;

struct Resources {
    std::int64_t mem_per_qpu_mb = 0;

    // Job allocation
    int job_cores = 0;                  // quantum_comm: total CPUs of the single job task
    std::int64_t job_mem_mb = 0;        // quantum_comm: --mem of the job
    std::int64_t mem_per_cpu_mb = 0;    // no/classical comm: --mem-per-cpu of the job

    // quantum_comm only (each vQPU server always takes one core)
    int executor_cores = 0;             // Cores of the executor (shared simulator)
    std::int64_t server_mem_mb = 0;     // Memory of EACH vQPU server
    std::int64_t servers_mem_mb = 0;    // Memory of the step with all the vQPU servers
    std::int64_t executor_mem_mb = 0;   // Memory of the executor
};

inline std::int64_t ceil_div(std::int64_t a, std::int64_t b)
{
    return (a + b - 1) / b;
}

// Parses a memory amount such as "512M", "1.5G", "2GB" or "4" (GB when there is no suffix,
// for backward compatibility) and returns it in MB, rounded up.
inline std::int64_t parse_memory_mb(const std::string& text)
{
    static const std::regex pattern(R"(^\s*(\d+(?:\.\d*)?|\.\d+)\s*(?:([KkMmGgTt])[Bb]?)?\s*$)");

    std::smatch match;
    if (!std::regex_match(text, match, pattern))
        throw std::invalid_argument("Invalid --mem-per-qpu value \"" + text + "\": expected a "
            "number with an optional unit suffix K, M, G or T (e.g. 512M, 1.5G, 4G). Without "
            "suffix the value is read as GB.");

    const double value = std::stod(match[1].str());
    const char unit = match[2].matched
        ? static_cast<char>(std::toupper(static_cast<unsigned char>(match[2].str()[0])))
        : 'G';

    double mb = value;
    switch (unit) {
        case 'K': mb = value / 1024.0; break;
        case 'M': mb = value; break;
        case 'G': mb = value * MB_PER_GB; break;
        case 'T': mb = value * MB_PER_GB * MB_PER_GB; break;
    }

    // The epsilon absorbs floating point noise (e.g. "0.1G") before rounding up
    return static_cast<std::int64_t>(std::ceil(mb - 1e-9));
}

inline Resources compute_resources(const CunqaArgs& args, Communications scheme)
{
    Resources res;

    if (args.cores_per_qpu < 1)
        throw std::invalid_argument("--cores-per-qpu must be at least 1.");

    const std::string mem_text = args.mem_per_qpu.value_or("15G");
    res.mem_per_qpu_mb = parse_memory_mb(mem_text);
    if (res.mem_per_qpu_mb < MIN_MEM_PER_QPU_MB)
        throw std::invalid_argument("--mem-per-qpu must be at least " +
            std::to_string(MIN_MEM_PER_QPU_MB) + "M per vQPU (got \"" + mem_text + "\" = " +
            std::to_string(res.mem_per_qpu_mb) + "M).");

    if (scheme != Communications::QC) {
        if (args.executor_cores.has_value())
            throw std::invalid_argument("--executor-cores can only be used with --quantum_comm.");

        // Rounded up, so each vQPU gets at least --mem-per-qpu
        res.mem_per_cpu_mb = ceil_div(res.mem_per_qpu_mb, args.cores_per_qpu);
        return res;
    }

    // ---- quantum_comm: n vQPU servers + one shared simulator process (executor) ----
    const int n = args.n_qpus;

    if (args.n_nodes.value_or(1) > 1 || args.qpus_per_node.has_value())
        throw std::invalid_argument("--quantum_comm families run on a single node: "
            "--n-nodes > 1 and --qpus-per-node are not supported.");

    if (args.executor_cores.has_value()) {
        res.executor_cores = args.executor_cores.value();
        if (res.executor_cores < 1)
            throw std::invalid_argument("--executor-cores must be at least 1.");
    } else {
        res.executor_cores = n * (args.cores_per_qpu - 1);
        if (res.executor_cores <= 0)
            throw std::invalid_argument("With --quantum_comm, cores_per_qpu must be equal or "
                "greater than 2 (one core for each vQPU server and the rest for the shared "
                "simulator), unless the simulator cores are set with --executor-cores.");
    }

    res.server_mem_mb = std::clamp(res.mem_per_qpu_mb / 4, MIN_SERVER_MEM_MB, MAX_SERVER_MEM_MB);
    res.servers_mem_mb = n * res.server_mem_mb;
    res.executor_mem_mb = n * (res.mem_per_qpu_mb - res.server_mem_mb);

    // A single task holding the CPUs of all steps, on a single node: the job memory is
    // exactly the sum of the memory of its steps
    res.job_cores = n + res.executor_cores;
    res.job_mem_mb = res.servers_mem_mb + res.executor_mem_mb;

    return res;
}
