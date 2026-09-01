#include "judge_service.h"
#include "cgroup_manager.h"
#include "logger.h"
#include "config.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <chrono>

namespace fs = std::filesystem;
using namespace std::chrono;

static std::string trimTrailingSpaces(const std::string& s) {
    size_t end = s.size();
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
        --end;
    }
    return s.substr(0, end);
}

JudgeService& JudgeService::instance() {
    static JudgeService instance_;
    return instance_;
}

std::string JudgeService::getSubmissionWorkDir(int submission_id) const {
    return "/tmp/oj_submissions/" + std::to_string(submission_id);
}

std::string JudgeService::ensureWorkDir(int submission_id) const {
    std::string work_dir = getSubmissionWorkDir(submission_id);
    fs::create_directories(work_dir);
    return work_dir;
}

bool JudgeService::readFile(const std::string& path, std::string& content) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return false;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    content = ss.str();
    return true;
}

bool JudgeService::readFileLimit(const std::string& path, std::string& content, size_t max_size) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }
    std::stringstream ss;
    char buffer[4096];
    size_t total_read = 0;
    while (ifs.read(buffer, sizeof(buffer))) {
        total_read += ifs.gcount();
        if (total_read > max_size) {
            content = "";
            return false;
        }
        ss.write(buffer, ifs.gcount());
    }
    total_read += ifs.gcount();
    if (total_read > max_size) {
        content = "";
        return false;
    }
    if (ifs.gcount() > 0) {
        ss.write(buffer, ifs.gcount());
    }
    content = ss.str();
    return true;
}

std::string JudgeService::getCompilerOutput(const std::string& error_path) {
    std::string output;
    readFile(error_path, output);
    return output;
}

bool JudgeService::compileCode(const Submission& submission, const Problem& problem,
                               std::string& error_detail, std::string& executable_path) {
    std::string work_dir = ensureWorkDir(submission.id);
    std::string source_path = work_dir + "/main.cpp";
    std::string error_path = work_dir + "/compile_error.txt";
    executable_path = work_dir + "/main";

    {
        std::ofstream ofs(source_path);
        if (!ofs.is_open()) {
            error_detail = "Failed to create source file";
            return false;
        }
        ofs << submission.code;
    }

    pid_t pid = fork();
    if (pid < 0) {
        error_detail = "Failed to fork compiler process";
        return false;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        dup2(devnull, STDERR_FILENO);
        close(devnull);

        int err_fd = open(error_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (err_fd >= 0) {
            dup2(err_fd, STDERR_FILENO);
            close(err_fd);
        }

        execlp("g++", "g++",
               "-O2",
               "-std=c++20",
               "-DONLINE_JUDGE",
               "-o", executable_path.c_str(),
               source_path.c_str(),
               "-lm",
               nullptr);

        _exit(1);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    bool compilation_success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);

    if (!compilation_success) {
        readFile(error_path, error_detail);
        if (error_detail.empty()) {
            error_detail = "Compilation failed with no error output";
        }
        return false;
    }

    struct stat st;
    if (stat(executable_path.c_str(), &st) != 0) {
        error_detail = "Compilation succeeded but executable not found";
        return false;
    }

    return true;
}

JudgeStatus JudgeService::parseVerdict(int exit_code, bool timed_out, bool oom,
                                       int64_t memory_kb, int64_t memory_limit_kb) {
    if (exit_code == 127) {
        return JudgeStatus::CE;
    }
    if (timed_out) {
        return JudgeStatus::TLE;
    }
    if (oom) {
        return JudgeStatus::MLE;
    }
    if (memory_limit_kb > 0 && memory_kb > memory_limit_kb) {
        return JudgeStatus::MLE;
    }
    if (WIFSIGNALED(exit_code)) {
        return JudgeStatus::RE;
    }
    if (exit_code != 0) {
        return JudgeStatus::RE;
    }
    return JudgeStatus::AC;
}

JudgeResult JudgeService::judgeSubmission(Submission& submission, const Problem& problem) {
    JudgeResult result;
    result.submission_id = submission.id;

    std::vector<TestCase> test_cases = TestCase::findByProblemId(problem.id);
    if (test_cases.empty()) {
        result.overall_status = JudgeStatus::UNKNOWN;
        result.error_detail = "No test cases found for problem";
        return result;
    }

    std::string executable_path;
    if (!compileCode(submission, problem, result.error_detail, executable_path)) {
        result.overall_status = JudgeStatus::CE;
        submission.status = judgeStatusToString(JudgeStatus::CE);
        submission.error_detail = result.error_detail;
        return result;
    }

    std::string cgroup_path;
    std::string cgroup_id = "sub_" + std::to_string(submission.id);
    if (CgroupManager::instance().isAvailable()) {
        if (CgroupManager::instance().createCgroup(cgroup_id)) {
            cgroup_path = CgroupManager::generateSubmissionCgroupPath(cgroup_id);
            CgroupManager::instance().setMemoryLimit(cgroup_path, problem.memory_limit_mb * 1024 * 1024);
            CgroupManager::instance().setCpuLimit(cgroup_path, problem.time_limit_ms * 1000, 1000000);
            CgroupManager::instance().setPidLimit(cgroup_path, 64);
        }
    }

    int total_time = 0;
    int peak_memory = 0;
    bool has_wa = false;
    bool has_pe = false;
    bool has_tle = false;
    bool has_mle = false;
    bool has_re = false;

    for (const auto& tc : test_cases) {
        TestCaseResult tc_result = runTestCase(executable_path, tc, problem);

        if (tc_result.status == JudgeStatus::TLE) has_tle = true;
        if (tc_result.status == JudgeStatus::MLE) has_mle = true;
        if (tc_result.status == JudgeStatus::WA) has_wa = true;
        if (tc_result.status == JudgeStatus::PE) has_pe = true;
        if (tc_result.status == JudgeStatus::RE) has_re = true;

        total_time += tc_result.execution_time_ms;
        if (tc_result.memory_kb > peak_memory) {
            peak_memory = tc_result.memory_kb;
        }

        result.test_case_results.push_back(tc_result);
    }

    if (!cgroup_path.empty()) {
        CgroupManager::instance().deleteCgroup(cgroup_path);
    }

    result.total_execution_time_ms = total_time;
    result.peak_memory_kb = peak_memory;

    if (has_tle) {
        result.overall_status = JudgeStatus::TLE;
    } else if (has_mle) {
        result.overall_status = JudgeStatus::MLE;
    } else if (has_re) {
        result.overall_status = JudgeStatus::RE;
    } else if (has_wa) {
        result.overall_status = JudgeStatus::WA;
    } else if (has_pe) {
        result.overall_status = JudgeStatus::PE;
    } else {
        result.overall_status = JudgeStatus::AC;
    }

    submission.status = judgeStatusToString(result.overall_status);
    submission.execute_time_ms = total_time;
    submission.execute_memory_kb = peak_memory;

    return result;
}

TestCaseResult JudgeService::runTestCase(const std::string& executable_path,
                                        const TestCase& test_case,
                                        const Problem& problem) {
    TestCaseResult result;
    result.test_case_id = test_case.id;

    std::string work_dir = fs::path(executable_path).parent_path().string();
    std::string output_path = work_dir + "/output_" + std::to_string(test_case.id) + ".txt";
    std::string error_path = work_dir + "/error_" + std::to_string(test_case.id) + ".txt";

    int64_t timeout_ms = problem.time_limit_ms;
    int64_t memory_limit_kb = problem.memory_limit_mb * 1024;

    CgroupResult cgroup_result;
    std::string cgroup_id = "tc_" + std::to_string(test_case.id);

    if (CgroupManager::instance().isAvailable()) {
        std::string cgroup_path = CgroupManager::generateSubmissionCgroupPath(cgroup_id);
        CgroupManager::instance().createCgroup(cgroup_id);
        CgroupManager::instance().setMemoryLimit(cgroup_path, memory_limit_kb * 1024);
        CgroupManager::instance().setCpuLimit(cgroup_path, timeout_ms * 1000, 1000000);
        CgroupManager::instance().setPidLimit(cgroup_path, 32);

        cgroup_result = CgroupManager::instance().executeInCgroup(
            cgroup_path, executable_path,
            test_case.input_path, output_path, error_path,
            timeout_ms, memory_limit_kb
        );

        CgroupManager::instance().deleteCgroup(cgroup_path);
    } else {
        int input_fd = -1;
        if (!test_case.input_path.empty() && test_case.input_path != "/dev/null") {
            input_fd = open(test_case.input_path.c_str(), O_RDONLY);
        }

        int output_fd = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        int error_fd = open(error_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

        auto start_time = steady_clock::now();

        pid_t pid = fork();
        if (pid == 0) {
            if (input_fd >= 0) {
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }
            dup2(output_fd, STDOUT_FILENO);
            dup2(error_fd, STDERR_FILENO);
            close(output_fd);
            close(error_fd);

            struct rlimit rl;
            rl.rlim_cur = timeout_ms / 1000 + 1;
            rl.rlim_max = timeout_ms / 1000 + 1;
            setrlimit(RLIMIT_CPU, &rl);

            if (memory_limit_kb > 0) {
                rl.rlim_cur = memory_limit_kb * 1024;
                rl.rlim_max = memory_limit_kb * 1024;
                setrlimit(RLIMIT_AS, &rl);
            }

            signal(SIGALRM, SIG_DFL);
            alarm(timeout_ms / 1000 + 1);

            execl(executable_path.c_str(), executable_path.c_str(), nullptr);
            _exit(127);
        }

        if (input_fd >= 0) close(input_fd);
        close(output_fd);
        close(error_fd);

        int status = 0;
        waitpid(pid, &status, 0);

        auto end_time = steady_clock::now();
        cgroup_result.execution_time_ms = duration_cast<milliseconds>(end_time - start_time).count();
        cgroup_result.timed_out = (cgroup_result.execution_time_ms >= timeout_ms);

        if (WIFEXITED(status)) {
            cgroup_result.exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            cgroup_result.exit_code = -1;
            cgroup_result.signal = WTERMSIG(status);
        }
    }

    result.execution_time_ms = static_cast<int>(cgroup_result.execution_time_ms);
    result.memory_kb = static_cast<int>(cgroup_result.memory_kb);

    result.status = parseVerdict(cgroup_result.exit_code, cgroup_result.timed_out,
                                cgroup_result.out_of_memory,
                                cgroup_result.memory_kb, memory_limit_kb);

    if (result.status == JudgeStatus::AC || result.status == JudgeStatus::WA || result.status == JudgeStatus::PE) {
        readFileLimit(test_case.output_path, result.expected_output, 10 * 1024 * 1024);
        readFileLimit(output_path, result.actual_output, 10 * 1024 * 1024);

        bool is_wa = false, is_pe = false;
        compareOutput(result.actual_output, result.expected_output, is_wa, is_pe);

        if (is_wa) {
            result.status = JudgeStatus::WA;
        } else if (is_pe) {
            result.status = JudgeStatus::PE;
        }
    }

    if (!cgroup_result.stderr_output.empty()) {
        result.error_detail = cgroup_result.stderr_output;
    }

    return result;
}

bool JudgeService::compareOutput(const std::string& actual, const std::string& expected,
                                  bool& is_wa, bool& is_pe) {
    is_wa = false;
    is_pe = false;

    std::string actual_trimmed = trimTrailingSpaces(actual);
    std::string expected_trimmed = trimTrailingSpaces(expected);

    if (actual_trimmed == expected_trimmed) {
        return true;
    }

    std::istringstream actual_ss(actual_trimmed);
    std::istringstream expected_ss(expected_trimmed);

    std::string actual_line, expected_line;
    bool lines_match = true;
    bool format_match = true;

    while (true) {
        bool actual_has = static_cast<bool>(std::getline(actual_ss, actual_line));
        bool expected_has = static_cast<bool>(std::getline(expected_ss, expected_line));

        if (!actual_has && !expected_has) break;

        if (actual_has != expected_has) {
            lines_match = false;
            format_match = false;
            break;
        }

        if (trimTrailingSpaces(actual_line) != trimTrailingSpaces(expected_line)) {
            lines_match = false;
            format_match = false;
            break;
        }
    }

    if (!lines_match) {
        std::istringstream a_ss(actual);
        std::istringstream e_ss(expected);
        std::string a_line, e_line;
        bool same_format = true;

        while (true) {
            bool a_has = static_cast<bool>(std::getline(a_ss, a_line));
            bool e_has = static_cast<bool>(std::getline(e_ss, e_line));

            if (!a_has && !e_has) break;

            std::string a_trim = trimTrailingSpaces(a_line);
            std::string e_trim = trimTrailingSpaces(e_line);

            if (a_trim != e_trim) {
                same_format = false;
                break;
            }
        }

        if (same_format) {
            is_pe = true;
        } else {
            is_wa = true;
        }
        return false;
    }

    return true;
}

std::string JudgeService::judgeStatusToString(JudgeStatus status) {
    switch (status) {
        case JudgeStatus::AC: return "AC";
        case JudgeStatus::WA: return "WA";
        case JudgeStatus::CE: return "CE";
        case JudgeStatus::RE: return "RE";
        case JudgeStatus::TLE: return "TLE";
        case JudgeStatus::MLE: return "MLE";
        case JudgeStatus::OLE: return "OLE";
        case JudgeStatus::PE: return "PE";
        default: return "UNKNOWN";
    }
}

JudgeStatus JudgeService::stringToJudgeStatus(const std::string& status) {
    if (status == "AC") return JudgeStatus::AC;
    if (status == "WA") return JudgeStatus::WA;
    if (status == "CE") return JudgeStatus::CE;
    if (status == "RE") return JudgeStatus::RE;
    if (status == "TLE") return JudgeStatus::TLE;
    if (status == "MLE") return JudgeStatus::MLE;
    if (status == "OLE") return JudgeStatus::OLE;
    if (status == "PE") return JudgeStatus::PE;
    return JudgeStatus::UNKNOWN;
}
