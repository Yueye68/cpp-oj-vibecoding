#include <gtest/gtest.h>
#include <mysql/mysql.h>
#include <ctime>
#include "db_pool/connection_pool.h"
#include "utils/config.h"
#include "models/user.h"
#include "models/problem.h"
#include "models/test_case.h"
#include "models/submission.h"
#include "models/submission_result.h"

class DbIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        DatabaseConfig dbConfig;
        dbConfig.host = "localhost";
        dbConfig.port = 3306;
        dbConfig.username = "root";
        dbConfig.password = "1";
        dbConfig.database = "oj_system";
        dbConfig.charset = "utf8mb4";

        ConnectionPool::instance().init(dbConfig, 3);
    }

    void TearDown() override {
        ConnectionPool::instance().close();
    }

    void cleanTable(const std::string& table) {
        MYSQL* conn = ConnectionPool::instance().getConnection();
        if (conn) {
            std::string query = "DELETE FROM " + table;
            mysql_real_query(conn, query.c_str(), query.size());
            ConnectionPool::instance().returnConnection(conn);
        }
    }

    int getTableCount(const std::string& table) {
        MYSQL* conn = ConnectionPool::instance().getConnection();
        if (!conn) return -1;

        std::string query = "SELECT COUNT(*) FROM " + table;
        if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
            ConnectionPool::instance().returnConnection(conn);
            return -1;
        }

        MYSQL_RES* result = mysql_store_result(conn);
        if (!result) {
            ConnectionPool::instance().returnConnection(conn);
            return -1;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        int count = row ? std::stoi(row[0]) : 0;
        mysql_free_result(result);
        ConnectionPool::instance().returnConnection(conn);
        return count;
    }
};

class UserDbTest : public DbIntegrationTest {
protected:
    void SetUp() override {
        DbIntegrationTest::SetUp();
        cleanTable("users");
    }
    void TearDown() override {
        cleanTable("users");
        DbIntegrationTest::TearDown();
    }
};

TEST_F(UserDbTest, SaveToDB_NewUser) {
    User user;
    user.username = "testuser";
    user.password_hash = "hashed_password";
    user.role = UserRole::User;

    EXPECT_TRUE(user.saveToDB());
    EXPECT_GT(user.id, 0);

    User loaded;
    EXPECT_TRUE(loaded.loadFromDB(user.id));
    EXPECT_EQ("testuser", loaded.username);
    EXPECT_EQ("hashed_password", loaded.password_hash);
    EXPECT_EQ(UserRole::User, loaded.role);
}

TEST_F(UserDbTest, SaveToDB_UpdateExistingUser) {
    User user;
    user.username = "original";
    user.password_hash = "original_hash";
    user.role = UserRole::User;

    EXPECT_TRUE(user.saveToDB());
    int originalId = user.id;

    user.username = "updated";
    user.password_hash = "updated_hash";
    user.role = UserRole::Admin;
    EXPECT_TRUE(user.saveToDB());
    EXPECT_EQ(originalId, user.id);

    User loaded;
    EXPECT_TRUE(loaded.loadFromDB(originalId));
    EXPECT_EQ("updated", loaded.username);
    EXPECT_EQ("updated_hash", loaded.password_hash);
    EXPECT_EQ(UserRole::Admin, loaded.role);
}

TEST_F(UserDbTest, LoadFromDB_NonExistent) {
    User user;
    EXPECT_FALSE(user.loadFromDB(99999));
}

TEST_F(UserDbTest, RemoveUser) {
    User user;
    user.username = "todelete";
    user.password_hash = "hash";
    user.role = UserRole::User;

    EXPECT_TRUE(user.saveToDB());
    int userId = user.id;

    EXPECT_TRUE(user.remove());
    EXPECT_FALSE(user.loadFromDB(userId));
}

class ProblemDbTest : public DbIntegrationTest {
protected:
    void SetUp() override {
        DbIntegrationTest::SetUp();
        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
    }
    void TearDown() override {
        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
        DbIntegrationTest::TearDown();
    }
};

TEST_F(ProblemDbTest, SaveToDB_NewProblem) {
    Problem problem;
    problem.title = "Two Sum";
    problem.description = "Given an array of integers...";
    problem.difficulty = Difficulty::Easy;
    problem.tags = {"array", "hash-table"};
    problem.time_limit_ms = 1000;
    problem.memory_limit_mb = 256;

    EXPECT_TRUE(problem.saveToDB());
    EXPECT_GT(problem.id, 0);

    Problem loaded;
    EXPECT_TRUE(loaded.loadFromDB(problem.id));
    EXPECT_EQ("Two Sum", loaded.title);
    EXPECT_EQ("Given an array of integers...", loaded.description);
    EXPECT_EQ(Difficulty::Easy, loaded.difficulty);
    EXPECT_EQ(2, loaded.tags.size());
    EXPECT_EQ(1000, loaded.time_limit_ms);
    EXPECT_EQ(256, loaded.memory_limit_mb);
}

TEST_F(ProblemDbTest, SaveToDB_UpdateExistingProblem) {
    Problem problem;
    problem.title = "Original Title";
    problem.description = "Original Description";
    problem.difficulty = Difficulty::Medium;

    EXPECT_TRUE(problem.saveToDB());
    int originalId = problem.id;

    problem.title = "Updated Title";
    problem.description = "Updated Description";
    problem.difficulty = Difficulty::Hard;
    EXPECT_TRUE(problem.saveToDB());
    EXPECT_EQ(originalId, problem.id);

    Problem loaded;
    EXPECT_TRUE(loaded.loadFromDB(originalId));
    EXPECT_EQ("Updated Title", loaded.title);
    EXPECT_EQ("Updated Description", loaded.description);
    EXPECT_EQ(Difficulty::Hard, loaded.difficulty);
}

TEST_F(ProblemDbTest, LoadFromDB_NonExistent) {
    Problem problem;
    EXPECT_FALSE(problem.loadFromDB(99999));
}

class TestCaseDbTest : public DbIntegrationTest {
protected:
    void SetUp() override {
        DbIntegrationTest::SetUp();
        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
    }
    void TearDown() override {
        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
        DbIntegrationTest::TearDown();
    }

    int createTestProblem() {
        Problem problem;
        problem.title = "Test Problem";
        problem.description = "Description";
        problem.difficulty = Difficulty::Easy;
        problem.saveToDB();
        return problem.id;
    }
};

TEST_F(TestCaseDbTest, SaveToDB_NewTestCase) {
    int problemId = createTestProblem();

    TestCase tc;
    tc.problem_id = problemId;
    tc.input_path = "/test_cases/input1.txt";
    tc.output_path = "/test_cases/output1.txt";
    tc.score = 100;
    tc.is_sample = true;

    EXPECT_TRUE(tc.saveToDB());
    EXPECT_GT(tc.id, 0);

    TestCase loaded;
    EXPECT_TRUE(loaded.loadFromDB(tc.id));
    EXPECT_EQ(problemId, loaded.problem_id);
    EXPECT_EQ("/test_cases/input1.txt", loaded.input_path);
    EXPECT_EQ("/test_cases/output1.txt", loaded.output_path);
    EXPECT_EQ(100, loaded.score);
    EXPECT_TRUE(loaded.is_sample);
}

TEST_F(TestCaseDbTest, SaveToDB_UpdateExistingTestCase) {
    int problemId = createTestProblem();

    TestCase tc;
    tc.problem_id = problemId;
    tc.input_path = "/old/input.txt";
    tc.output_path = "/old/output.txt";
    tc.score = 50;
    tc.is_sample = false;

    EXPECT_TRUE(tc.saveToDB());
    int originalId = tc.id;

    tc.input_path = "/new/input.txt";
    tc.output_path = "/new/output.txt";
    tc.score = 100;
    tc.is_sample = true;
    EXPECT_TRUE(tc.saveToDB());
    EXPECT_EQ(originalId, tc.id);

    TestCase loaded;
    EXPECT_TRUE(loaded.loadFromDB(originalId));
    EXPECT_EQ("/new/input.txt", loaded.input_path);
    EXPECT_EQ("/new/output.txt", loaded.output_path);
    EXPECT_EQ(100, loaded.score);
    EXPECT_TRUE(loaded.is_sample);
}

class SubmissionDbTest : public DbIntegrationTest {
protected:
    void SetUp() override {
        DbIntegrationTest::SetUp();
        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
        cleanTable("users");
    }
    void TearDown() override {
        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
        cleanTable("users");
        DbIntegrationTest::TearDown();
    }

    int createTestUser() {
        User user;
        user.username = "testuser_" + std::to_string(time(nullptr));
        user.password_hash = "hash";
        user.role = UserRole::User;
        user.saveToDB();
        return user.id;
    }

    int createTestProblem() {
        Problem problem;
        problem.title = "Test Problem";
        problem.description = "Description";
        problem.difficulty = Difficulty::Easy;
        problem.saveToDB();
        return problem.id;
    }
};

TEST_F(SubmissionDbTest, SaveToDB_NewSubmission) {
    int userId = createTestUser();
    int problemId = createTestProblem();

    Submission sub;
    sub.user_id = userId;
    sub.problem_id = problemId;
    sub.code = "#include <bits/stdc++.h>";
    sub.language = "cpp";
    sub.status = "PENDING";

    EXPECT_TRUE(sub.saveToDB());
    EXPECT_GT(sub.id, 0);

    Submission loaded;
    EXPECT_TRUE(loaded.loadFromDB(sub.id));
    EXPECT_EQ(userId, loaded.user_id);
    EXPECT_EQ(problemId, loaded.problem_id);
    EXPECT_EQ("#include <bits/stdc++.h>", loaded.code);
    EXPECT_EQ("cpp", loaded.language);
    EXPECT_EQ("PENDING", loaded.status);
}

TEST_F(SubmissionDbTest, SaveToDB_UpdateSubmissionResult) {
    int userId = createTestUser();
    int problemId = createTestProblem();

    Submission sub;
    sub.user_id = userId;
    sub.problem_id = problemId;
    sub.code = "#include <bits/stdc++.h>";
    sub.status = "PENDING";

    EXPECT_TRUE(sub.saveToDB());

    sub.status = "AC";
    sub.execute_time_ms = 50;
    sub.execute_memory_kb = 1024;
    EXPECT_TRUE(sub.saveToDB());

    Submission loaded;
    EXPECT_TRUE(loaded.loadFromDB(sub.id));
    EXPECT_EQ("AC", loaded.status);
    EXPECT_EQ(50, loaded.execute_time_ms);
    EXPECT_EQ(1024, loaded.execute_memory_kb);
}

class SubmissionResultDbTest : public DbIntegrationTest {
protected:
    void SetUp() override {
        DbIntegrationTest::SetUp();
        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
        cleanTable("users");
    }
    void TearDown() override {
        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
        cleanTable("users");
        DbIntegrationTest::TearDown();
    }

    int createTestUser() {
        User user;
        user.username = "testuser_" + std::to_string(time(nullptr));
        user.password_hash = "hash";
        user.role = UserRole::User;
        user.saveToDB();
        return user.id;
    }

    int createTestProblem() {
        Problem problem;
        problem.title = "Test Problem";
        problem.description = "Description";
        problem.difficulty = Difficulty::Easy;
        problem.saveToDB();
        return problem.id;
    }

    int createTestSubmission() {
        int userId = createTestUser();
        int problemId = createTestProblem();

        Submission sub;
        sub.user_id = userId;
        sub.problem_id = problemId;
        sub.code = "#include <bits/stdc++.h>";
        sub.status = "PENDING";
        sub.saveToDB();
        return sub.id;
    }

    int createTestTestCase() {
        int problemId = createTestProblem();

        TestCase tc;
        tc.problem_id = problemId;
        tc.input_path = "/input.txt";
        tc.output_path = "/output.txt";
        tc.score = 100;
        tc.is_sample = true;
        tc.saveToDB();
        return tc.id;
    }
};

TEST_F(SubmissionResultDbTest, SaveToDB_NewSubmissionResult) {
    int submissionId = createTestSubmission();
    int testCaseId = createTestTestCase();

    SubmissionResult sr;
    sr.submission_id = submissionId;
    sr.test_case_id = testCaseId;
    sr.status = "AC";
    sr.expected_output = "42";
    sr.actual_output = "42";
    sr.execute_time_ms = 10;
    sr.execute_memory_kb = 512;

    EXPECT_TRUE(sr.saveToDB());
    EXPECT_GT(sr.id, 0);

    SubmissionResult loaded;
    EXPECT_TRUE(loaded.loadFromDB(sr.id));
    EXPECT_EQ(submissionId, loaded.submission_id);
    EXPECT_EQ(testCaseId, loaded.test_case_id);
    EXPECT_EQ("AC", loaded.status);
    EXPECT_EQ("42", loaded.expected_output);
    EXPECT_EQ("42", loaded.actual_output);
    EXPECT_EQ(10, loaded.execute_time_ms);
    EXPECT_EQ(512, loaded.execute_memory_kb);
}

TEST_F(SubmissionResultDbTest, SaveToDB_UpdateSubmissionResult) {
    int submissionId = createTestSubmission();
    int testCaseId = createTestTestCase();

    SubmissionResult sr;
    sr.submission_id = submissionId;
    sr.test_case_id = testCaseId;
    sr.status = "WA";
    sr.expected_output = "42";
    sr.actual_output = "41";

    EXPECT_TRUE(sr.saveToDB());

    sr.status = "AC";
    sr.actual_output = "42";
    sr.execute_time_ms = 20;
    EXPECT_TRUE(sr.saveToDB());

    SubmissionResult loaded;
    EXPECT_TRUE(loaded.loadFromDB(sr.id));
    EXPECT_EQ("AC", loaded.status);
    EXPECT_EQ("42", loaded.actual_output);
    EXPECT_EQ(20, loaded.execute_time_ms);
}