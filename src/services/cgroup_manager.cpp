#include "cgroup_manager.h"
#include "logger.h"
#include "config.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

namespace fs = std::filesystem;
using namespace std::chrono;

CgroupManager& CgroupManager::instance() {
    static CgroupManager instance_;
    return instance_;
}

bool CgroupManager::isAvailable() const {
    return use_cgroup_;
}

std::string CgroupManager::getCgroupRoot() {
    return "/sys/fs/cgroup";
}

std::string CgroupManager::generateSubmissionCgroupPath(const std::string& submission_id) {
    return getCgroupRoot() + "/oj_submissions/" + submission_id;
}

bool CgroupManager::writeCgroupFile(const std::string& path, const std::string& value) {
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        Logger::instance().error("Failed to open cgroup file for writing: " + path);
        return false;
    }
    ofs << value;
    ofs.close();
    if (ofs.fail()) {
        Logger::instance().error("Failed to write cgroup file: " + path);
        return false;
    }
    return true;
}

std::string CgroupManager::readCgroupFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return "";
    }
    std::string content;
    std::getline(ifs, content);
    return content;
}

bool CgroupManager::createCgroup(const std::string& submission_id) {
    std::string cgroup_path = generateSubmissionCgroupPath(submission_id);

    try {
        fs::create_directories(cgroup_path);
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to create cgroup directory: " + std::string(e.what()));
        return false;
    }

    if (!fs::exists(cgroup_path + "/cgroup.procs")) {
        Logger::instance().error("Cgroup directory does not have cgroup.procs: " + cgroup_path);
        return false;
    }

    use_cgroup_ = true;
    return true;
}

bool CgroupManager::deleteCgroup(const std::string& cgroup_path) {
    if (!fs::exists(cgroup_path)) {
        return true;
    }

    try {
        std::ofstream ofs(cgroup_path + "/cgroup.procs");
        if (ofs.is_open()) {
            std::string line;
            std::ifstream ifs(cgroup_path + "/cgroup.procs");
            while (std::getline(ifs, line)) {
                try {
                    pid_t pid = static_cast<pid_t>(std::stoi(line));
                    kill(pid, SIGKILL);
                } catch (...) {
                }
            }
        }

        fs::remove_all(cgroup_path);
    } catch (const std::exception& e) {
        Logger::instance().warn("Failed to delete cgroup: " + std::string(e.what()));
        return false;
    }

    return true;
}

bool CgroupManager::setCpuLimit(const std::string& cgroup_path, int64_t quota_us, int64_t period_us) {
    if (quota_us > 0) {
        if (!writeCgroupFile(cgroup_path + "/cpu.max",
                           std::to_string(quota_us) + " " + std::to_string(period_us))) {
            return false;
        }
    }
    return true;
}

bool CgroupManager::setMemoryLimit(const std::string& cgroup_path, int64_t limit_bytes) {
    if (limit_bytes > 0) {
        if (!writeCgroupFile(cgroup_path + "/memory.max", std::to_string(limit_bytes))) {
            return false;
        }
        if (!writeCgroupFile(cgroup_path + "/memory.swap.max", "0")) {
            return false;
        }
    }
    return true;
}

bool CgroupManager::setPidLimit(const std::string& cgroup_path, int64_t max_pids) {
    if (max_pids > 0) {
        if (!writeCgroupFile(cgroup_path + "/pids.max", std::to_string(max_pids))) {
            return false;
        }
    }
    return true;
}

bool CgroupManager::addTask(const std::string& cgroup_path, pid_t pid) {
    return writeCgroupFile(cgroup_path + "/cgroup.procs", std::to_string(pid));
}

int64_t CgroupManager::getMemoryUsage(const std::string& cgroup_path) {
    std::string current = readCgroupFile(cgroup_path + "/memory.current");
    if (current.empty()) return 0;
    try {
        return std::stoll(current);
    } catch (...) {
        return 0;
    }
}

int64_t CgroupManager::getPeakMemoryUsage(const std::string& cgroup_path) {
    std::string peak = readCgroupFile(cgroup_path + "/memory.peak");
    if (peak.empty()) return 0;
    try {
        return std::stoll(peak);
    } catch (...) {
        return 0;
    }
}

CgroupResult CgroupManager::executeInCgroup(
    const std::string& cgroup_path,
    const std::string& executable_path,
    const std::string& input_path,
    const std::string& output_path,
    const std::string& error_path,
    int64_t timeout_ms,
    int64_t memory_limit_kb
) {
    CgroupResult result;

    int input_fd = -1;
    int output_fd = -1;
    int error_fd = -1;

    if (!input_path.empty() && input_path != "/dev/null") {
        input_fd = open(input_path.c_str(), O_RDONLY);
        if (input_fd < 0) {
            Logger::instance().error("Failed to open input file: " + input_path);
            return result;
        }
    }

    output_fd = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        if (input_fd >= 0) close(input_fd);
        Logger::instance().error("Failed to open output file: " + output_path);
        return result;
    }

    error_fd = open(error_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (error_fd < 0) {
        if (input_fd >= 0) close(input_fd);
        close(output_fd);
        Logger::instance().error("Failed to open error file: " + error_path);
        return result;
    }

    auto start_time = steady_clock::now();

    pid_t pid = fork();
    if (pid < 0) {
        Logger::instance().error("Failed to fork process: " + std::string(strerror(errno)));
        if (input_fd >= 0) close(input_fd);
        close(output_fd);
        close(error_fd);
        return result;
    }

    if (pid == 0) {
        if (input_fd >= 0) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        dup2(output_fd, STDOUT_FILENO);
        dup2(error_fd, STDERR_FILENO);
        close(output_fd);
        close(error_fd);

        if (!cgroup_path.empty() && use_cgroup_) {
            std::string procs_path = cgroup_path + "/cgroup.procs";
            FILE* f = fopen(procs_path.c_str(), "w");
            if (f) {
                fprintf(f, "%d", getpid());
                fclose(f);
            }
        }

        struct rlimit rl;
        rl.rlim_cur = timeout_ms / 1000 + 1;
        rl.rlim_max = timeout_ms / 1000 + 1;
        setrlimit(RLIMIT_CPU, &rl);

        if (memory_limit_kb > 0) {
            rl.rlim_cur = memory_limit_kb * 1024;
            rl.rlim_max = memory_limit_kb * 1024;
            setrlimit(RLIMIT_AS, &rl);
        }

        rl.rlim_cur = 1;
        rl.rlim_max = 1;
        setrlimit(RLIMIT_NPROC, &rl);

        rl.rlim_cur = 1024 * 1024;
        rl.rlim_max = 1024 * 1024;
        setrlimit(RLIMIT_FSIZE, &rl);

        signal(SIGALRM, SIG_DFL);
        alarm(timeout_ms / 1000 + 1);

        execl(executable_path.c_str(), executable_path.c_str(), nullptr);

        _exit(127);
    }

    if (input_fd >= 0) close(input_fd);
    close(output_fd);
    close(error_fd);

    if (!cgroup_path.empty() && use_cgroup_) {
        for (int i = 0; i < 10; i++) {
            usleep(10000);
            if (addTask(cgroup_path, pid)) break;
        }
    }

    int status = 0;
    pid_t wait_result = 0;
    int64_t peak_memory_kb = 0;

    while (true) {
        wait_result = waitpid(pid, &status, WNOHANG);
        if (wait_result != 0) break;

        auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start_time).count();
        if (elapsed > timeout_ms + 1000) {
            kill(pid, SIGKILL);
            result.timed_out = true;
            break;
        }

        if (!cgroup_path.empty() && use_cgroup_) {
            int64_t current_mem = getMemoryUsage(cgroup_path) / 1024;
            if (current_mem > peak_memory_kb) {
                peak_memory_kb = current_mem;
            }
            if (memory_limit_kb > 0 && current_mem > memory_limit_kb) {
                kill(pid, SIGKILL);
                result.out_of_memory = true;
                break;
            }
        }

        usleep(1000);
    }

    if (wait_result == 0) {
        wait_result = waitpid(pid, &status, 0);
    }

    auto end_time = steady_clock::now();
    result.execution_time_ms = duration_cast<milliseconds>(end_time - start_time).count();

    if (!cgroup_path.empty() && use_cgroup_) {
        int64_t final_mem = getPeakMemoryUsage(cgroup_path) / 1024;
        if (final_mem > peak_memory_kb) {
            peak_memory_kb = final_mem;
        }
        result.memory_kb = peak_memory_kb;
    }

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = -1;
        result.signal = WTERMSIG(status);
        if (result.signal == SIGKILL && !result.timed_out && !result.out_of_memory) {
            result.timed_out = true;
        }
    }

    {
        std::ifstream ifs(output_path);
        if (ifs.is_open()) {
            std::stringstream ss;
            ss << ifs.rdbuf();
            result.stdout_output = ss.str();
        }
    }

    {
        std::ifstream ifs(error_path);
        if (ifs.is_open()) {
            std::stringstream ss;
            ss << ifs.rdbuf();
            result.stderr_output = ss.str();
        }
    }

    return result;
}
