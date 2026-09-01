#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <sys/types.h>

struct CgroupResult {
    int exit_code = -1;
    int signal = 0;
    int64_t execution_time_ms = 0;
    int64_t memory_kb = 0;
    std::string stdout_output;
    std::string stderr_output;
    bool timed_out = false;
    bool out_of_memory = false;
    bool out_of_pids = false;
};

class CgroupManager {
public:
    static CgroupManager& instance();

    bool isAvailable() const;

    bool createCgroup(const std::string& submission_id);
    bool deleteCgroup(const std::string& cgroup_path);

    bool setCpuLimit(const std::string& cgroup_path, int64_t quota_us, int64_t period_us);
    bool setMemoryLimit(const std::string& cgroup_path, int64_t limit_bytes);
    bool setPidLimit(const std::string& cgroup_path, int64_t max_pids);

    CgroupResult executeInCgroup(
        const std::string& cgroup_path,
        const std::string& executable_path,
        const std::string& input_path,
        const std::string& output_path,
        const std::string& error_path,
        int64_t timeout_ms,
        int64_t memory_limit_kb
    );

    static std::string getCgroupRoot();
    static std::string generateSubmissionCgroupPath(const std::string& submission_id);

private:
    CgroupManager() = default;
    ~CgroupManager() = default;
    CgroupManager(const CgroupManager&) = delete;
    CgroupManager& operator=(const CgroupManager&) = delete;

    bool writeCgroupFile(const std::string& path, const std::string& value);
    std::string readCgroupFile(const std::string& path);
    bool addTask(const std::string& cgroup_path, pid_t pid);
    int64_t getMemoryUsage(const std::string& cgroup_path);
    int64_t getPeakMemoryUsage(const std::string& cgroup_path);

    bool use_cgroup_ = false;
    std::string cgroup_root_ = "/sys/fs/cgroup";
};
