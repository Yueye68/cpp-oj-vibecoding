#include <gtest/gtest.h>
#include <optional>
#include <vector>
#include "models/user.h"
#include "models/problem.h"
#include "models/test_case.h"
#include "models/submission.h"
#include "models/submission_result.h"

class UserModelTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(UserModelTest, RoleToString) {
    EXPECT_EQ("user", User::roleToString(UserRole::User));
    EXPECT_EQ("admin", User::roleToString(UserRole::Admin));
}

TEST_F(UserModelTest, StringToRole) {
    EXPECT_EQ(UserRole::User, User::stringToRole("user"));
    EXPECT_EQ(UserRole::Admin, User::stringToRole("admin"));
    EXPECT_EQ(UserRole::User, User::stringToRole("unknown"));
    EXPECT_EQ(UserRole::User, User::stringToRole(""));
}

TEST_F(UserModelTest, DefaultConstruction) {
    User user;
    EXPECT_EQ(0, user.id);
    EXPECT_EQ("", user.username);
    EXPECT_EQ("", user.password_hash);
    EXPECT_EQ(UserRole::User, user.role);
    EXPECT_EQ("", user.created_at);
}

TEST_F(UserModelTest, StructAssignment) {
    User user;
    user.id = 1;
    user.username = "testuser";
    user.password_hash = "hash123";
    user.role = UserRole::Admin;
    user.created_at = "2024-01-01 00:00:00";

    EXPECT_EQ(1, user.id);
    EXPECT_EQ("testuser", user.username);
    EXPECT_EQ("hash123", user.password_hash);
    EXPECT_EQ(UserRole::Admin, user.role);
    EXPECT_EQ("2024-01-01 00:00:00", user.created_at);
}

class ProblemModelTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ProblemModelTest, DifficultyToString) {
    EXPECT_EQ("easy", Problem::difficultyToString(Difficulty::Easy));
    EXPECT_EQ("medium", Problem::difficultyToString(Difficulty::Medium));
    EXPECT_EQ("hard", Problem::difficultyToString(Difficulty::Hard));
}

TEST_F(ProblemModelTest, StringToDifficulty) {
    EXPECT_EQ(Difficulty::Easy, Problem::stringToDifficulty("easy"));
    EXPECT_EQ(Difficulty::Medium, Problem::stringToDifficulty("medium"));
    EXPECT_EQ(Difficulty::Hard, Problem::stringToDifficulty("hard"));
    EXPECT_EQ(Difficulty::Easy, Problem::stringToDifficulty("unknown"));
    EXPECT_EQ(Difficulty::Easy, Problem::stringToDifficulty(""));
}

TEST_F(ProblemModelTest, TagsToJson_Empty) {
    std::vector<std::string> tags;
    EXPECT_EQ("[]", Problem::tagsToJson(tags));
}

TEST_F(ProblemModelTest, TagsToJson_SingleTag) {
    std::vector<std::string> tags = {"algorithm"};
    EXPECT_EQ("[\"algorithm\"]", Problem::tagsToJson(tags));
}

TEST_F(ProblemModelTest, TagsToJson_MultipleTags) {
    std::vector<std::string> tags = {"algorithm", "dynamic-programming", "greedy"};
    std::string result = Problem::tagsToJson(tags);
    EXPECT_TRUE(result.find("\"algorithm\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"dynamic-programming\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"greedy\"") != std::string::npos);
}

TEST_F(ProblemModelTest, JsonToTags_Empty) {
    EXPECT_TRUE(Problem::jsonToTags("").empty());
    EXPECT_TRUE(Problem::jsonToTags("null").empty());
    EXPECT_TRUE(Problem::jsonToTags("[]").empty());
}

TEST_F(ProblemModelTest, JsonToTags_SingleTag) {
    auto tags = Problem::jsonToTags("[\"algorithm\"]");
    EXPECT_EQ(1, tags.size());
    EXPECT_EQ("algorithm", tags[0]);
}

TEST_F(ProblemModelTest, JsonToTags_MultipleTags) {
    auto tags = Problem::jsonToTags("[\"tag1\", \"tag2\", \"tag3\"]");
    EXPECT_EQ(3, tags.size());
    EXPECT_EQ("tag1", tags[0]);
    EXPECT_EQ("tag2", tags[1]);
    EXPECT_EQ("tag3", tags[2]);
}

TEST_F(ProblemModelTest, JsonToTags_WithSpaces) {
    auto tags = Problem::jsonToTags("[\"tag with space\", \"another tag\"]");
    EXPECT_EQ(2, tags.size());
    EXPECT_EQ("tag with space", tags[0]);
    EXPECT_EQ("another tag", tags[1]);
}

TEST_F(ProblemModelTest, DefaultConstruction) {
    Problem problem;
    EXPECT_EQ(0, problem.id);
    EXPECT_EQ("", problem.title);
    EXPECT_EQ("", problem.description);
    EXPECT_EQ(Difficulty::Easy, problem.difficulty);
    EXPECT_TRUE(problem.tags.empty());
    EXPECT_EQ(1000, problem.time_limit_ms);
    EXPECT_EQ(256, problem.memory_limit_mb);
    EXPECT_EQ(0, problem.test_case_count);
}

TEST_F(ProblemModelTest, StructAssignment) {
    Problem problem;
    problem.id = 100;
    problem.title = "Two Sum";
    problem.description = "Given an array of integers...";
    problem.difficulty = Difficulty::Medium;
    problem.tags = {"array", "hash-table"};
    problem.time_limit_ms = 2000;
    problem.memory_limit_mb = 128;
    problem.test_case_count = 10;

    EXPECT_EQ(100, problem.id);
    EXPECT_EQ("Two Sum", problem.title);
    EXPECT_EQ("Given an array of integers...", problem.description);
    EXPECT_EQ(Difficulty::Medium, problem.difficulty);
    EXPECT_EQ(2, problem.tags.size());
    EXPECT_EQ(2000, problem.time_limit_ms);
    EXPECT_EQ(128, problem.memory_limit_mb);
    EXPECT_EQ(10, problem.test_case_count);
}

TEST_F(ProblemModelTest, TagsRoundTrip) {
    std::vector<std::string> originalTags = {"algorithm", "data-structure", "tree"};
    std::string json = Problem::tagsToJson(originalTags);
    std::vector<std::string> parsedTags = Problem::jsonToTags(json);

    EXPECT_EQ(originalTags.size(), parsedTags.size());
    for (size_t i = 0; i < originalTags.size(); ++i) {
        EXPECT_EQ(originalTags[i], parsedTags[i]);
    }
}

class TestCaseModelTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestCaseModelTest, DefaultConstruction) {
    TestCase tc;
    EXPECT_EQ(0, tc.id);
    EXPECT_EQ(0, tc.problem_id);
    EXPECT_EQ("", tc.input_path);
    EXPECT_EQ("", tc.output_path);
    EXPECT_EQ(100, tc.score);
    EXPECT_FALSE(tc.is_sample);
}

TEST_F(TestCaseModelTest, StructAssignment) {
    TestCase tc;
    tc.id = 1;
    tc.problem_id = 100;
    tc.input_path = "/test_cases/100/input1.txt";
    tc.output_path = "/test_cases/100/output1.txt";
    tc.score = 50;
    tc.is_sample = true;

    EXPECT_EQ(1, tc.id);
    EXPECT_EQ(100, tc.problem_id);
    EXPECT_EQ("/test_cases/100/input1.txt", tc.input_path);
    EXPECT_EQ("/test_cases/100/output1.txt", tc.output_path);
    EXPECT_EQ(50, tc.score);
    EXPECT_TRUE(tc.is_sample);
}

TEST_F(TestCaseModelTest, IsSampleToggle) {
    TestCase tc;
    tc.is_sample = false;
    EXPECT_FALSE(tc.is_sample);

    tc.is_sample = true;
    EXPECT_TRUE(tc.is_sample);
}

class SubmissionModelTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SubmissionModelTest, DefaultConstruction) {
    Submission sub;
    EXPECT_EQ(0, sub.id);
    EXPECT_EQ(0, sub.user_id);
    EXPECT_EQ(0, sub.problem_id);
    EXPECT_EQ("", sub.code);
    EXPECT_EQ("cpp", sub.language);
    EXPECT_EQ("", sub.status);
    EXPECT_EQ("", sub.error_detail);
    EXPECT_EQ(0, sub.execute_time_ms);
    EXPECT_EQ(0, sub.execute_memory_kb);
    EXPECT_EQ("", sub.created_at);
}

TEST_F(SubmissionModelTest, StructAssignment) {
    Submission sub;
    sub.id = 1;
    sub.user_id = 10;
    sub.problem_id = 100;
    sub.code = "#include <iostream>";
    sub.language = "cpp";
    sub.status = "AC";
    sub.error_detail = "";
    sub.execute_time_ms = 50;
    sub.execute_memory_kb = 1024;
    sub.created_at = "2024-01-01 12:00:00";

    EXPECT_EQ(1, sub.id);
    EXPECT_EQ(10, sub.user_id);
    EXPECT_EQ(100, sub.problem_id);
    EXPECT_EQ("#include <iostream>", sub.code);
    EXPECT_EQ("cpp", sub.language);
    EXPECT_EQ("AC", sub.status);
    EXPECT_EQ(50, sub.execute_time_ms);
    EXPECT_EQ(1024, sub.execute_memory_kb);
}

TEST_F(SubmissionModelTest, StatusValues) {
    Submission sub;

    std::vector<std::string> validStatuses = {"AC", "WA", "CE", "RE", "TLE", "MLE", "OLE", "PE"};
    for (const auto& status : validStatuses) {
        sub.status = status;
        EXPECT_EQ(status, sub.status);
    }
}

class SubmissionResultModelTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SubmissionResultModelTest, DefaultConstruction) {
    SubmissionResult sr;
    EXPECT_EQ(0, sr.id);
    EXPECT_EQ(0, sr.submission_id);
    EXPECT_EQ(0, sr.test_case_id);
    EXPECT_EQ("", sr.status);
    EXPECT_EQ("", sr.actual_output);
    EXPECT_EQ("", sr.expected_output);
    EXPECT_EQ(0, sr.execute_time_ms);
    EXPECT_EQ(0, sr.execute_memory_kb);
}

TEST_F(SubmissionResultModelTest, StructAssignment) {
    SubmissionResult sr;
    sr.id = 1;
    sr.submission_id = 100;
    sr.test_case_id = 10;
    sr.status = "AC";
    sr.actual_output = "Hello World";
    sr.expected_output = "Hello World";
    sr.execute_time_ms = 25;
    sr.execute_memory_kb = 512;

    EXPECT_EQ(1, sr.id);
    EXPECT_EQ(100, sr.submission_id);
    EXPECT_EQ(10, sr.test_case_id);
    EXPECT_EQ("AC", sr.status);
    EXPECT_EQ("Hello World", sr.actual_output);
    EXPECT_EQ("Hello World", sr.expected_output);
    EXPECT_EQ(25, sr.execute_time_ms);
    EXPECT_EQ(512, sr.execute_memory_kb);
}

TEST_F(SubmissionResultModelTest, WAStatusWithOutput) {
    SubmissionResult sr;
    sr.status = "WA";
    sr.expected_output = "42";
    sr.actual_output = "41";

    EXPECT_EQ("WA", sr.status);
    EXPECT_EQ("42", sr.expected_output);
    EXPECT_EQ("41", sr.actual_output);
}

