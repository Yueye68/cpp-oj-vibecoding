#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "services/judge_service.h"

class JudgeStatusTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JudgeStatusTest, JudgeStatusToString) {
    EXPECT_EQ("AC", JudgeService::judgeStatusToString(JudgeStatus::AC));
    EXPECT_EQ("WA", JudgeService::judgeStatusToString(JudgeStatus::WA));
    EXPECT_EQ("CE", JudgeService::judgeStatusToString(JudgeStatus::CE));
    EXPECT_EQ("RE", JudgeService::judgeStatusToString(JudgeStatus::RE));
    EXPECT_EQ("TLE", JudgeService::judgeStatusToString(JudgeStatus::TLE));
    EXPECT_EQ("MLE", JudgeService::judgeStatusToString(JudgeStatus::MLE));
    EXPECT_EQ("OLE", JudgeService::judgeStatusToString(JudgeStatus::OLE));
    EXPECT_EQ("PE", JudgeService::judgeStatusToString(JudgeStatus::PE));
    EXPECT_EQ("UNKNOWN", JudgeService::judgeStatusToString(JudgeStatus::UNKNOWN));
}

TEST_F(JudgeStatusTest, StringToJudgeStatus) {
    EXPECT_EQ(JudgeStatus::AC, JudgeService::stringToJudgeStatus("AC"));
    EXPECT_EQ(JudgeStatus::WA, JudgeService::stringToJudgeStatus("WA"));
    EXPECT_EQ(JudgeStatus::CE, JudgeService::stringToJudgeStatus("CE"));
    EXPECT_EQ(JudgeStatus::RE, JudgeService::stringToJudgeStatus("RE"));
    EXPECT_EQ(JudgeStatus::TLE, JudgeService::stringToJudgeStatus("TLE"));
    EXPECT_EQ(JudgeStatus::MLE, JudgeService::stringToJudgeStatus("MLE"));
    EXPECT_EQ(JudgeStatus::OLE, JudgeService::stringToJudgeStatus("OLE"));
    EXPECT_EQ(JudgeStatus::PE, JudgeService::stringToJudgeStatus("PE"));
    EXPECT_EQ(JudgeStatus::UNKNOWN, JudgeService::stringToJudgeStatus("UNKNOWN"));
    EXPECT_EQ(JudgeStatus::UNKNOWN, JudgeService::stringToJudgeStatus("invalid"));
    EXPECT_EQ(JudgeStatus::UNKNOWN, JudgeService::stringToJudgeStatus(""));
}

TEST_F(JudgeStatusTest, ParseVerdict_ExitCode127_IsCE) {
    EXPECT_EQ(JudgeStatus::CE, JudgeService::parseVerdict(127, false, false, 0, 0));
}

TEST_F(JudgeStatusTest, ParseVerdict_TimedOut_IsTLE) {
    EXPECT_EQ(JudgeStatus::TLE, JudgeService::parseVerdict(0, true, false, 0, 0));
    EXPECT_EQ(JudgeStatus::TLE, JudgeService::parseVerdict(1, true, false, 0, 0));
}

TEST_F(JudgeStatusTest, ParseVerdict_OOM_IsMLE) {
    EXPECT_EQ(JudgeStatus::MLE, JudgeService::parseVerdict(0, false, true, 0, 0));
    EXPECT_EQ(JudgeStatus::MLE, JudgeService::parseVerdict(0, false, true, 100000, 50000));
}

TEST_F(JudgeStatusTest, ParseVerdict_MemoryExceedsLimit_IsMLE) {
    EXPECT_EQ(JudgeStatus::MLE, JudgeService::parseVerdict(0, false, false, 60000, 50000));
    EXPECT_EQ(JudgeStatus::AC, JudgeService::parseVerdict(0, false, false, 40000, 50000));
}

TEST_F(JudgeStatusTest, ParseVerdict_WIFSIGNALED_IsRE) {
    EXPECT_EQ(JudgeStatus::RE, JudgeService::parseVerdict(-1, false, false, 0, 0));
    EXPECT_EQ(JudgeStatus::AC, JudgeService::parseVerdict(0, false, false, 0, 0));
}

TEST_F(JudgeStatusTest, ParseVerdict_NonZeroExitCode_IsRE) {
    EXPECT_EQ(JudgeStatus::RE, JudgeService::parseVerdict(1, false, false, 0, 0));
    EXPECT_EQ(JudgeStatus::RE, JudgeService::parseVerdict(139, false, false, 0, 0));
}

TEST_F(JudgeStatusTest, ParseVerdict_ZeroExitCode_IsAC) {
    EXPECT_EQ(JudgeStatus::AC, JudgeService::parseVerdict(0, false, false, 0, 0));
    EXPECT_EQ(JudgeStatus::AC, JudgeService::parseVerdict(0, false, false, 1000, 5000));
}

class TestCaseResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestCaseResultTest, DefaultConstruction) {
    TestCaseResult result;
    EXPECT_EQ(0, result.test_case_id);
    EXPECT_EQ(JudgeStatus::UNKNOWN, result.status);
    EXPECT_EQ("", result.actual_output);
    EXPECT_EQ("", result.expected_output);
    EXPECT_EQ(0, result.execution_time_ms);
    EXPECT_EQ(0, result.memory_kb);
    EXPECT_EQ("", result.error_detail);
}

TEST_F(TestCaseResultTest, StructAssignment) {
    TestCaseResult result;
    result.test_case_id = 5;
    result.status = JudgeStatus::AC;
    result.actual_output = "Hello World";
    result.expected_output = "Hello World";
    result.execution_time_ms = 100;
    result.memory_kb = 4096;
    result.error_detail = "";

    EXPECT_EQ(5, result.test_case_id);
    EXPECT_EQ(JudgeStatus::AC, result.status);
    EXPECT_EQ("Hello World", result.actual_output);
    EXPECT_EQ("Hello World", result.expected_output);
    EXPECT_EQ(100, result.execution_time_ms);
    EXPECT_EQ(4096, result.memory_kb);
}

TEST_F(TestCaseResultTest, VariousStatuses) {
    std::vector<JudgeStatus> statuses = {
        JudgeStatus::AC, JudgeStatus::WA, JudgeStatus::CE, JudgeStatus::RE,
        JudgeStatus::TLE, JudgeStatus::MLE, JudgeStatus::OLE, JudgeStatus::PE
    };

    for (const auto& status : statuses) {
        TestCaseResult result;
        result.status = status;
        EXPECT_EQ(status, result.status);
    }
}

class JudgeResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JudgeResultTest, DefaultConstruction) {
    JudgeResult result;
    EXPECT_EQ(0, result.submission_id);
    EXPECT_EQ(JudgeStatus::UNKNOWN, result.overall_status);
    EXPECT_EQ("", result.error_detail);
    EXPECT_EQ(0, result.total_execution_time_ms);
    EXPECT_EQ(0, result.peak_memory_kb);
    EXPECT_TRUE(result.test_case_results.empty());
}

TEST_F(JudgeResultTest, StructAssignment) {
    JudgeResult result;
    result.submission_id = 123;
    result.overall_status = JudgeStatus::WA;
    result.error_detail = "Wrong answer on test case 1";
    result.total_execution_time_ms = 500;
    result.peak_memory_kb = 8192;

    TestCaseResult tc_result;
    tc_result.test_case_id = 1;
    tc_result.status = JudgeStatus::WA;
    result.test_case_results.push_back(tc_result);

    EXPECT_EQ(123, result.submission_id);
    EXPECT_EQ(JudgeStatus::WA, result.overall_status);
    EXPECT_EQ("Wrong answer on test case 1", result.error_detail);
    EXPECT_EQ(500, result.total_execution_time_ms);
    EXPECT_EQ(8192, result.peak_memory_kb);
    EXPECT_EQ(1, result.test_case_results.size());
    EXPECT_EQ(1, result.test_case_results[0].test_case_id);
}

TEST_F(JudgeResultTest, MultipleTestCaseResults) {
    JudgeResult result;
    result.submission_id = 456;

    std::vector<JudgeStatus> statuses = {
        JudgeStatus::AC, JudgeStatus::AC, JudgeStatus::WA, JudgeStatus::TLE
    };

    for (size_t i = 0; i < statuses.size(); ++i) {
        TestCaseResult tc_result;
        tc_result.test_case_id = static_cast<int>(i + 1);
        tc_result.status = statuses[i];
        result.test_case_results.push_back(tc_result);
    }

    EXPECT_EQ(4, result.test_case_results.size());
    EXPECT_EQ(JudgeStatus::AC, result.test_case_results[0].status);
    EXPECT_EQ(JudgeStatus::AC, result.test_case_results[1].status);
    EXPECT_EQ(JudgeStatus::WA, result.test_case_results[2].status);
    EXPECT_EQ(JudgeStatus::TLE, result.test_case_results[3].status);
}
