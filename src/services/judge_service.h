#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "problem.h"
#include "test_case.h"
#include "submission.h"
#include "submission_result.h"

enum class JudgeStatus {
    AC,
    WA,
    CE,
    RE,
    TLE,
    MLE,
    OLE,
    PE,
    UNKNOWN
};

struct TestCaseResult {
    int test_case_id = 0;
    JudgeStatus status = JudgeStatus::UNKNOWN;
    std::string actual_output;
    std::string expected_output;
    int execution_time_ms = 0;
    int memory_kb = 0;
    std::string error_detail;
};

struct JudgeResult {
    int submission_id = 0;
    JudgeStatus overall_status = JudgeStatus::UNKNOWN;
    std::string error_detail;
    int total_execution_time_ms = 0;
    int peak_memory_kb = 0;
    std::vector<TestCaseResult> test_case_results;
};

class JudgeService {
public:
    static JudgeService& instance();

    JudgeResult judgeSubmission(Submission& submission, const Problem& problem);

    static std::string judgeStatusToString(JudgeStatus status);
    static JudgeStatus stringToJudgeStatus(const std::string& status);
    static JudgeStatus parseVerdict(int exit_code, bool timed_out, bool oom, int64_t memory_kb, int64_t memory_limit_kb);

    static std::string getCompilerOutput(const std::string& error_path);

private:
    JudgeService() = default;
    ~JudgeService() = default;
    JudgeService(const JudgeService&) = delete;
    JudgeService& operator=(const JudgeService&) = delete;

    bool compileCode(const Submission& submission, const Problem& problem,
                     std::string& error_detail, std::string& executable_path);
    TestCaseResult runTestCase(const std::string& executable_path,
                               const TestCase& test_case,
                               const Problem& problem);
    bool compareOutput(const std::string& actual, const std::string& expected,
                       bool& is_wa, bool& is_pe);

    std::string getSubmissionWorkDir(int submission_id) const;
    std::string ensureWorkDir(int submission_id) const;

    static bool readFile(const std::string& path, std::string& content);
    static bool readFileLimit(const std::string& path, std::string& content, size_t max_size);

    std::string compile_work_dir_ = "/tmp/oj_compile";
};
