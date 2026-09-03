#include <gtest/gtest.h>
#include "server/handlers/submission_handler.h"
#include "server/middleware/auth_middleware.h"
#include "models/submission.h"
#include "models/submission_result.h"
#include "models/test_case.h"
#include "models/problem.h"
#include "models/user.h"
#include "models/judge_queue.h"
#include "db_pool/connection_pool.h"
#include "utils/config.h"
#include "utils/session.h"
#include "utils/json.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

class SubmissionHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        DatabaseConfig dbConfig;
        dbConfig.host = "localhost";
        dbConfig.port = 3306;
        dbConfig.username = "root";
        dbConfig.password = "1";
        dbConfig.database = "oj_system";
        dbConfig.charset = "utf8mb4";

        ConnectionPool::instance().init(dbConfig, 5);
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

    int createAdminUser() {
        User user;
        user.username = "admin_" + std::to_string(time(nullptr));
        user.password_hash = "hashed";
        user.role = UserRole::Admin;
        user.saveToDB();
        return user.id;
    }

    int createNormalUser() {
        User user;
        user.username = "user_" + std::to_string(time(nullptr));
        user.password_hash = "hashed";
        user.role = UserRole::User;
        user.saveToDB();
        return user.id;
    }

    std::string adminToken;
    std::string userToken;

    static std::string generateToken() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<long long> dist(0, 9223372036854775807LL);
        return std::to_string(dist(gen));
    }
};

class SubmissionHandlerDbTest : public SubmissionHandlerTest {
protected:
    void SetUp() override {
        SubmissionHandlerTest::SetUp();

        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
        cleanTable("users");

        int adminId = createAdminUser();
        int userId = createNormalUser();

        User adminUser;
        adminUser.id = adminId;
        adminUser.username = "admin_test";
        adminUser.role = UserRole::Admin;
        adminToken = SessionManager::instance().createSession(adminUser);

        User normalUser;
        normalUser.id = userId;
        normalUser.username = "user_test";
        normalUser.role = UserRole::User;
        userToken = SessionManager::instance().createSession(normalUser);
    }

    void TearDown() override {
        if (!adminToken.empty()) SessionManager::instance().destroySession(adminToken);
        if (!userToken.empty()) SessionManager::instance().destroySession(userToken);

        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
        cleanTable("users");

        SubmissionHandlerTest::TearDown();
    }

    int createProblem(const std::string& title, Difficulty difficulty = Difficulty::Easy,
                      const std::vector<std::string>& tags = {"array"}) {
        Problem p;
        p.title = title;
        p.description = "Test description for " + title;
        p.difficulty = difficulty;
        p.tags = tags;
        p.time_limit_ms = 5000;
        p.memory_limit_mb = 256;
        p.saveToDB();
        return p.id;
    }

    int createTestCase(int problemId, const std::string& input, const std::string& output, bool isSample = false) {
        std::string testCaseDir = "./test_cases/" + std::to_string(problemId);
        if (!fs::exists(testCaseDir)) {
            fs::create_directories(testCaseDir);
        }

        std::string inputPath = testCaseDir + "/" + std::to_string(time(nullptr)) + "_in.txt";
        std::string outputPath = testCaseDir + "/" + std::to_string(time(nullptr)) + "_out.txt";

        std::ofstream inputOut(inputPath);
        inputOut << input;
        inputOut.close();

        std::ofstream outputOut(outputPath);
        outputOut << output;
        outputOut.close();

        TestCase tc;
        tc.problem_id = problemId;
        tc.input_path = inputPath;
        tc.output_path = outputPath;
        tc.score = 100;
        tc.is_sample = isSample;
        tc.create();
        return tc.id;
    }

    httplib::Request createRequest(const std::string& method, const std::string& path,
                                   const std::string& body = "",
                                   const std::string& cookie = "") {
        httplib::Request req;
        req.method = method;

        size_t query_start = path.find('?');
        if (query_start != std::string::npos) {
            req.path = path.substr(0, query_start);
            std::string query_str = path.substr(query_start + 1);
            std::istringstream iss(query_str);
            std::string param;
            while (std::getline(iss, param, '&')) {
                size_t eq_pos = param.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = param.substr(0, eq_pos);
                    std::string value = param.substr(eq_pos + 1);
                    req.params.emplace(key, value);
                }
            }
        } else {
            req.path = path;
        }

        req.body = body;
        if (!cookie.empty()) {
            req.headers.emplace("Cookie", cookie);
        }
        return req;
    }

    std::string createSubmissionJson(int problemId, const std::string& code, const std::string& lang = "cpp") {
        Json::Value json;
        json["problem_id"] = problemId;
        json["code"] = code;
        json["language"] = lang;
        return json.toStyledString();
    }
};

TEST_F(SubmissionHandlerDbTest, CreateSubmission_WithoutAuth) {
    int problemId = createProblem("Test Problem");
    std::string body = createSubmissionJson(problemId, "#include <iostream>");

    httplib::Request req = createRequest("POST", "/api/submissions", body);
    httplib::Response res;

    handleCreateSubmission(req, res);

    EXPECT_EQ(401, res.status);
}

TEST_F(SubmissionHandlerDbTest, CreateSubmission_InvalidJson) {
    int problemId = createProblem("Test Problem");

    httplib::Request req = createRequest("POST", "/api/submissions", "not json", "session_token=" + adminToken);
    httplib::Response res;

    handleCreateSubmission(req, res);

    EXPECT_EQ(400, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);
    EXPECT_EQ("Invalid JSON", json["error"].asString());
}

TEST_F(SubmissionHandlerDbTest, CreateSubmission_MissingFields) {
    int problemId = createProblem("Test Problem");

    Json::Value json;
    json["problem_id"] = problemId;
    json["code"] = "#include <iostream>";

    httplib::Request req = createRequest("POST", "/api/submissions", json.toStyledString(), "session_token=" + adminToken);
    httplib::Response res;

    handleCreateSubmission(req, res);

    EXPECT_EQ(400, res.status);

    Json::Value resJson;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &resJson, nullptr);
    EXPECT_EQ("Missing required fields", resJson["error"].asString());
}

TEST_F(SubmissionHandlerDbTest, CreateSubmission_ProblemNotFound) {
    std::string body = createSubmissionJson(99999, "#include <iostream>");

    httplib::Request req = createRequest("POST", "/api/submissions", body, "session_token=" + adminToken);
    httplib::Response res;

    handleCreateSubmission(req, res);

    EXPECT_EQ(404, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);
    EXPECT_EQ("Problem not found", json["error"].asString());
}

TEST_F(SubmissionHandlerDbTest, CreateSubmission_Success) {
    int problemId = createProblem("Test Problem");
    createTestCase(problemId, "1 2 3", "6");
    std::string body = createSubmissionJson(problemId, R"(
#include <bits/stdc++.h>
using namespace std;
int main() {
    int a, b, c;
    cin >> a >> b >> c;
    cout << (a + b + c) << endl;
    return 0;
})");

    httplib::Request req = createRequest("POST", "/api/submissions", body, "session_token=" + adminToken);
    httplib::Response res;

    handleCreateSubmission(req, res);

    EXPECT_EQ(201, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_GT(json["id"].asInt(), 0);
    EXPECT_EQ("pending", json["status"].asString());
    EXPECT_EQ("pending", json["queue_status"].asString());

    int submissionId = json["id"].asInt();
    auto submissionOpt = Submission::findById(submissionId);
    EXPECT_TRUE(submissionOpt.has_value());
    EXPECT_EQ(problemId, submissionOpt->problem_id);
    EXPECT_EQ("pending", submissionOpt->queue_status);
}

TEST_F(SubmissionHandlerDbTest, CreateSubmission_QueuesJob) {
    int problemId = createProblem("Test Problem");
    createTestCase(problemId, "1 2 3", "6");
    std::string body = createSubmissionJson(problemId, R"(
#include <bits/stdc++.h>
using namespace std;
int main() {
    int a, b, c;
    cin >> a >> b >> c;
    cout << (a + b + c) << endl;
    return 0;
})");

    httplib::Request req = createRequest("POST", "/api/submissions", body, "session_token=" + userToken);
    httplib::Response res;

    handleCreateSubmission(req, res);

    EXPECT_EQ(201, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("pending", json["status"].asString());

    int submissionId = json["id"].asInt();
    auto items = JudgeQueueItem::findBySubmissionId(submissionId);
    EXPECT_EQ(1, items.size());
    EXPECT_EQ("pending", items[0].status);
}

TEST_F(SubmissionHandlerDbTest, ListSubmissions_WithoutAuth) {
    httplib::Request req = createRequest("GET", "/api/submissions");
    httplib::Response res;

    handleListSubmissions(req, res);

    EXPECT_EQ(401, res.status);
}

TEST_F(SubmissionHandlerDbTest, ListSubmissions_Empty) {
    httplib::Request req = createRequest("GET", "/api/submissions", "", "session_token=" + userToken);
    httplib::Response res;

    handleListSubmissions(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_TRUE(json["submissions"].isArray());
    EXPECT_EQ(0, json["submissions"].size());
    EXPECT_EQ(0, json["total"]);
}

TEST_F(SubmissionHandlerDbTest, ListSubmissions_UserSeesOwnOnly) {
    int problemId = createProblem("Test Problem");

    User normalUser;
    normalUser.username = "user_own";
    normalUser.role = UserRole::User;
    normalUser.saveToDB();

    Submission sub1;
    sub1.user_id = normalUser.id;
    sub1.problem_id = problemId;
    sub1.code = "#include <iostream>";
    sub1.language = "cpp";
    sub1.status = "AC";
    sub1.create();

    Submission sub2;
    sub2.user_id = 99999;
    sub2.problem_id = problemId;
    sub2.code = "#include <vector>";
    sub2.language = "cpp";
    sub2.status = "WA";
    sub2.create();

    httplib::Request req = createRequest("GET", "/api/submissions", "", "session_token=" + userToken);
    httplib::Response res;

    handleListSubmissions(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ(0, json["total"]);
}

TEST_F(SubmissionHandlerDbTest, ListSubmissions_AdminSeesAll) {
    int problemId = createProblem("Test Problem");

    User adminUser;
    adminUser.username = "admin_list";
    adminUser.role = UserRole::Admin;
    adminUser.saveToDB();
    std::string adminSession = SessionManager::instance().createSession(adminUser);

    User normalUser;
    normalUser.username = "user_list";
    normalUser.role = UserRole::User;
    normalUser.saveToDB();
    std::string userSession = SessionManager::instance().createSession(normalUser);

    Submission sub1;
    sub1.user_id = adminUser.id;
    sub1.problem_id = problemId;
    sub1.code = "#include <iostream>";
    sub1.language = "cpp";
    sub1.status = "AC";
    sub1.create();

    Submission sub2;
    sub2.user_id = normalUser.id;
    sub2.problem_id = problemId;
    sub2.code = "#include <vector>";
    sub2.language = "cpp";
    sub2.status = "WA";
    sub2.create();

    httplib::Request req = createRequest("GET", "/api/submissions", "", "session_token=" + adminSession);
    httplib::Response res;

    handleListSubmissions(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ(2, json["total"]);

    SessionManager::instance().destroySession(adminSession);
    SessionManager::instance().destroySession(userSession);
}

TEST_F(SubmissionHandlerDbTest, GetSubmission_WithoutAuth) {
    httplib::Request req = createRequest("GET", "/api/submissions/1");
    httplib::Response res;

    handleGetSubmission(req, res);

    EXPECT_EQ(401, res.status);
}

TEST_F(SubmissionHandlerDbTest, GetSubmission_NotFound) {
    httplib::Request req = createRequest("GET", "/api/submissions/99999", "", "session_token=" + adminToken);
    req.path_params["id"] = "99999";
    httplib::Response res;

    handleGetSubmission(req, res);

    EXPECT_EQ(404, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);
    EXPECT_EQ("Submission not found", json["error"].asString());
}

TEST_F(SubmissionHandlerDbTest, GetSubmission_Forbidden) {
    int problemId = createProblem("Test Problem");

    User otherUser;
    otherUser.username = "other_user";
    otherUser.role = UserRole::User;
    otherUser.saveToDB();

    Submission sub;
    sub.user_id = otherUser.id;
    sub.problem_id = problemId;
    sub.code = "#include <iostream>";
    sub.language = "cpp";
    sub.status = "AC";
    sub.create();

    httplib::Request req = createRequest("GET", "/api/submissions/" + std::to_string(sub.id), "", "session_token=" + userToken);
    req.path_params["id"] = std::to_string(sub.id);
    httplib::Response res;

    handleGetSubmission(req, res);

    EXPECT_EQ(403, res.status);
}

TEST_F(SubmissionHandlerDbTest, GetSubmission_OwnSubmission) {
    int problemId = createProblem("Test Problem");

    User normalUser;
    normalUser.username = "user_own_sub";
    normalUser.role = UserRole::User;
    normalUser.saveToDB();
    std::string userSession = SessionManager::instance().createSession(normalUser);

    Submission sub;
    sub.user_id = normalUser.id;
    sub.problem_id = problemId;
    sub.code = "#include <iostream>";
    sub.language = "cpp";
    sub.status = "AC";
    sub.create();

    httplib::Request req = createRequest("GET", "/api/submissions/" + std::to_string(sub.id), "", "session_token=" + userSession);
    req.path_params["id"] = std::to_string(sub.id);
    httplib::Response res;

    handleGetSubmission(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ(sub.id, json["id"].asInt());
    EXPECT_EQ(problemId, json["problem_id"].asInt());
    EXPECT_EQ("AC", json["status"].asString());

    SessionManager::instance().destroySession(userSession);
}

TEST_F(SubmissionHandlerDbTest, GetSubmission_IncludesResults) {
    int problemId = createProblem("Test Problem");
    int tcId = createTestCase(problemId, "1 2 3", "6");

    User adminUser;
    adminUser.username = "admin_results";
    adminUser.role = UserRole::Admin;
    adminUser.saveToDB();
    std::string adminSession = SessionManager::instance().createSession(adminUser);

    Submission sub;
    sub.user_id = adminUser.id;
    sub.problem_id = problemId;
    sub.code = "#include <iostream>";
    sub.language = "cpp";
    sub.status = "AC";
    sub.create();

    SubmissionResult sr;
    sr.submission_id = sub.id;
    sr.test_case_id = tcId;
    sr.status = "AC";
    sr.actual_output = "6";
    sr.expected_output = "6";
    sr.execute_time_ms = 10;
    sr.execute_memory_kb = 1024;
    sr.create();

    httplib::Request req = createRequest("GET", "/api/submissions/" + std::to_string(sub.id), "", "session_token=" + adminSession);
    req.path_params["id"] = std::to_string(sub.id);
    httplib::Response res;

    handleGetSubmission(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_TRUE(json["results"].isArray());
    EXPECT_EQ(1, json["results"].size());
    EXPECT_EQ(tcId, json["results"][0]["test_case_id"].asInt());
    EXPECT_EQ("AC", json["results"][0]["status"].asString());

    SessionManager::instance().destroySession(adminSession);
}

TEST_F(SubmissionHandlerDbTest, GetSubmission_AdminCanSeeAny) {
    int problemId = createProblem("Test Problem");

    User otherUser;
    otherUser.username = "other_for_admin";
    otherUser.role = UserRole::User;
    otherUser.saveToDB();

    Submission sub;
    sub.user_id = otherUser.id;
    sub.problem_id = problemId;
    sub.code = "#include <iostream>";
    sub.language = "cpp";
    sub.status = "AC";
    sub.create();

    httplib::Request req = createRequest("GET", "/api/submissions/" + std::to_string(sub.id), "", "session_token=" + adminToken);
    req.path_params["id"] = std::to_string(sub.id);
    httplib::Response res;

    handleGetSubmission(req, res);

    EXPECT_EQ(200, res.status);
}