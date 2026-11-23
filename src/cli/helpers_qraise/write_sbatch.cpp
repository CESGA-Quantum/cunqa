#include <fstream>
#include <string>
#include <cstdio> // For popen, pclose
#include <algorithm>
#include <filesystem>
#include <stdexcept>

#include "write_sbatch.hpp"
#include "utils_qraise.hpp"
#include "args_qraise.hpp"

#include "utils/json.hpp"
#include "utils/constants.hpp"

#include "logger.hpp"

namespace {

void write_sbatch_header(std::ofstream& sbatchFile, const CunqaArgs& args, Communications scheme,
                         const Resources& res)
{
    sbatchFile << "#!/bin/bash\n";
    sbatchFile << "#SBATCH --job-name=qraise \n";

    if (scheme == Communications::QC) {
        // A single task holding the CPUs of all steps, so that the steps (n vQPU servers + the
        // executor) always fit in the job and no step asks for more cpus-per-task than the job.
        sbatchFile << "#SBATCH --ntasks=1\n";
        sbatchFile << "#SBATCH -c " << res.job_cores << "\n";
        sbatchFile << "#SBATCH -N 1\n";
    } else {
        sbatchFile << "#SBATCH --ntasks=" << args.n_qpus << "\n";
        sbatchFile << "#SBATCH -c " << args.cores_per_qpu << "\n";

        if (args.n_nodes.has_value())
            sbatchFile << "#SBATCH -N " << args.n_nodes.value() << "\n";

        if (args.qpus_per_node.has_value())
            sbatchFile << "#SBATCH --ntasks-per-node=" << args.qpus_per_node.value() << "\n";
    }

    if (args.partition.has_value())
            sbatchFile << "#SBATCH --partition=" << args.partition.value() << "\n";

    if (args.node_list.has_value()) {
        sbatchFile << "#SBATCH --nodelist=";

        const auto& nodes = args.node_list.value();
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (i > 0)
                sbatchFile << ",";
            sbatchFile << nodes[i];
        }
        sbatchFile << "\n";
    }

    if (scheme == Communications::QC)
        sbatchFile << "#SBATCH --mem=" << res.job_mem_mb << "M\n";
    else
        sbatchFile << "#SBATCH --mem-per-cpu=" << res.mem_per_cpu_mb << "M\n";

    sbatchFile << "#SBATCH --time=" << args.time << "\n";
    sbatchFile << "#SBATCH --output=qraise_%j\n\n";
    
    if (args.gpu || args.gpu_name.has_value()) {
        if (args.simulator != "Aer")
            throw std::invalid_argument("At this moment, only Aer supports GPU simulation");
        const int number_of_gpus = scheme == Communications::QC ? 1 : args.n_qpus;
        
        if (args.gpu_name.has_value())
            sbatchFile << "#SBATCH --gres=gpu:" << args.gpu_name.value() << ":" << number_of_gpus << "\n";
        else
            sbatchFile << "#SBATCH --gres=gpu:" << number_of_gpus << "\n";
    }

    sbatchFile << "unset SLURM_MEM_PER_CPU SLURM_MEM_PER_NODE SLURM_CPU_BIND_LIST SLURM_CPU_BIND\n";
    sbatchFile << "EPILOG_PATH=" << cunqa::INSTALL_PATH << "/bin/epilog.sh\n";
}

void write_run_command(std::ofstream& sbatchFile,
                       const CunqaArgs& args,
                       Communications scheme,
                       const Resources& res)
{
    auto subcommand =
        std::string(args.co_located ? "co_located" : "hpc") + " " +
        std::string(get_communications_name(scheme)) + " " +
        args.family + " " + args.simulator;

    if (args.infrastructure.has_value()) { 
        subcommand += " " + args.infrastructure.value();
    } else if (args.backend.has_value()) {
        subcommand += " " + args.backend.value();
    }

    if (scheme == Communications::NC) {
        sbatchFile << "srun --task-epilog=$EPILOG_PATH setup_qpus " << subcommand << "\n";
    } else if (scheme == Communications::CC) {
        sbatchFile << "srun";
    #ifdef USE_MPI_BTW_QPU
        sbatchFile << " --mpi=pmix";
    #endif
        sbatchFile << " --task-epilog=$EPILOG_PATH setup_qpus " << subcommand << '\n';
    } else if (scheme == Communications::QC) {

        std::string executor_gres = "--gres=none ";
        if (args.gpu || args.gpu_name.has_value()) {
            if (!args.gpu_name.has_value())
                executor_gres = "--gres=gpu:1 ";
            else
                executor_gres = "--gres=gpu:" + args.gpu_name.value() + ":1 ";
        }

        // vQPU servers: one core each. Note: --overcommit does not make the tasks of a step share
        // CPUs (each task is still bound to its own core), so it can not be used to pack them.
        sbatchFile
            << "srun --exact --exclusive -n " << args.n_qpus << " -c 1 "
            << "--mem=" << res.servers_mem_mb << "M "
            << "--gres=none --task-epilog=$EPILOG_PATH setup_qpus " << subcommand << " &\n";

        sbatchFile
            << "srun --exact --exclusive " << executor_gres
            << "-n 1 -c " << res.executor_cores << " "
            << "--mem=" << res.executor_mem_mb << "M "
            << "setup_executor " << args.simulator << " " << args.n_qpus << "\n";
    }
}


void write_qmio(std::ofstream& sbatchFile, const CunqaArgs& args)
{
    sbatchFile << "#!/bin/bash\n";
    sbatchFile << "#SBATCH --job-name=qraise \n";
    sbatchFile << "#SBATCH --partition qpu \n";
    sbatchFile << "#SBATCH --ntasks=1 \n";
    sbatchFile << "#SBATCH -c 2 \n";
    sbatchFile << "#SBATCH --mem-per-cpu=15G \n";
    sbatchFile << "#SBATCH --time=" << args.time << "\n";

    sbatchFile << "#SBATCH --output=qraise_%j\n";

    sbatchFile << "\n\n";

    sbatchFile << "unset SLURM_MEM_PER_CPU SLURM_MEM_PER_NODE SLURM_CPU_BIND_LIST SLURM_CPU_BIND\n";
    sbatchFile << "EPILOG_PATH=" << cunqa::INSTALL_PATH << "/bin/epilog.sh\n";

    sbatchFile << "\n\n";
    sbatchFile << "srun --task-epilog=$EPILOG_PATH setup_qmio " + args.family;
}

} // End of anonymous namespace

void write_sbatch(std::ofstream& sbatchFile, const CunqaArgs& args)
{
    if (args.cc && args.qc)
        LOGGER_WARN("cc and qc both selected: using qc.");

    const std::string_view scheme_name = 
        args.cc ? "cc" :
        args.qc ? "qc" : "nc";
    auto scheme = get_communications_class(scheme_name);

    if (args.n_qpus == 0 || args.time == "")
        throw std::invalid_argument("qraise needs two mandatory arguments:\n \t " 
            "-n: number of vQPUs to be raised\n\t "
            "-t: maximum time vQPUs will be raised (hh:mm:ss)\n");

    if (!communication_supported_by_simulator(args.simulator, scheme))
        throw std::invalid_argument("Simulator " + args.simulator + 
            " is not available for " + get_communications_name(scheme) + 
            " communications simulation. Aborting.");

    if (!valid_family_name(args.family))
        throw std::invalid_argument("Invalid family name " + args.family + 
            ": it can not be empty, start with '.' or contain '/'.");

    if (exists_family_name(args.family))
        throw std::runtime_error("There are QPUs with the same family name as the provided: " +
            args.family + ".");

    if (!args.qmio) {
        const Resources res = compute_resources(args, scheme);
        write_sbatch_header(sbatchFile, args, scheme, res);
        write_run_command(sbatchFile, args, scheme, res);
    } else
        write_qmio(sbatchFile, args);
}