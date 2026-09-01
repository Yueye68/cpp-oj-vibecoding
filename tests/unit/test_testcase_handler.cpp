#include <gtest/gtest.h>
#include "server/handlers/testcase_handler.h"
#include "server/middleware/auth_middleware.h"
#include "models/test_case.h"
#include "models/problem.h"
#include "models/user.h"
#include "db_pool/connection_pool.h"
#include "utils/config.h"
#include "utils/session.h"
#include "utils/json.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

class TestCaseHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        DatabaseConfig dbConfig;
        dbConfig.host = "localhost";
        dbConfig.port = 3306;
        dbConfig.username = "root";
        dbConfig.password = "";
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

class TestCaseHandlerDbTest : public TestCaseHandlerTest {
protected:
    void SetUp() override {
        TestCaseHandlerTest::SetUp();

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

        TestCaseHandlerTest::TearDown();
    }

    int createProblem(const std::string& title, Difficulty difficulty = Difficulty::Easy,
                      const std::vector<std::string>& tags = {"array"}) {
        Problem p;
        p.title = title;
        p.description = "Test description for " + title;
        p.difficulty = difficulty;
        p.tags = tags;
        p.time_limit_ms = 1000;
        p.memory_limit_mb = 256;
        p.saveToDB();
        return p.id;
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

    httplib::Request createMultipartRequest(const std::string& method, const std::string& path,
                                            const std::string& cookie = "") {
        httplib::Request req;
        req.method = method;
        req.path = path;
        if (!cookie.empty()) {
            req.headers.emplace("Cookie", cookie);
        }
        return req;
    }

    void addFileToRequest(httplib::Request& req, const std::string& fieldname,
                          const std::string& filename, const std::string& content) {
        req.form.files.emplace(fieldname, httplib::FormData{
            fieldname, content, filename, "text/plain", {}
        });
    }
};

TEST_F(TestCaseHandlerDbTest, GetTestCasesByProblem_Empty) {
    int problemId = createProblem("Test Problem");

    httplib::Request req = createRequest("GET", "/api/problems/" + std::to_string(problemId) + "/testcases");
    req.path_params["id"] = std::to_string(problemId);
    httplib::Response res;

    handleGetTestCasesByProblem(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    Json::String err;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, &err);

    EXPECT_TRUE(json["test_cases"].isArray());
    EXPECT_EQ(0, json["test_cases"].size());
    EXPECT_EQ(0, json["count"]);
}

TEST_F(TestCaseHandlerDbTest, GetTestCasesByProblem_WithData) {
    int problemId = createProblem("Test Problem");

    TestCase tc1;
    tc1.problem_id = problemId;
    tc1.input_path = "/tmp/test_cases/" + std::to_string(problemId) + "/1_in.txt";
    tc1.output_path = "/tmp/test_cases/" + std::to_string(problemId) + "/1_out.txt";
    tc1.score = 100;
    tc1.is_sample = true;
    tc1.create();

    TestCase tc2;
    tc2.problem_id = problemId;
    tc2.input_path = "/tmp/test_cases/" + std::to_string(problemId) + "/2_in.txt";
    tc2.output_path = "/tmp/test_cases/" + std::to_string(problemId) + "/2_out.txt";
    tc2.score = 100;
    tc2.is_sample = false;
    tc2.create();

    httplib::Request req = createRequest("GET", "/api/problems/" + std::to_string(problemId) + "/testcases");
    req.path_params["id"] = std::to_string(problemId);
    httplib::Response res;

    handleGetTestCasesByProblem(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ(2, json["test_cases"].size());
    EXPECT_EQ(2, json["count"]);

    bool foundSample = false;
    bool foundNonSample = false;
    for (const auto& tc : json["test_cases"]) {
        if (tc["is_sample"].asBool()) foundSample = true;
        else foundNonSample = true;
    }
    EXPECT_TRUE(foundSample);
    EXPECT_TRUE(foundNonSample);
}

TEST_F(TestCaseHandlerDbTest, GetTestCasesByProblem_ProblemNotFound) {
    httplib::Request req = createRequest("GET", "/api/problems/99999/testcases");
    req.path_params["id"] = "99999";
    httplib::Response res;

    handleGetTestCasesByProblem(req, res);

    EXPECT_EQ(404, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("Problem not found", json["error"].asString());
}

TEST_F(TestCaseHandlerDbTest, AddTestCase_Success) {
    int problemId = createProblem("Test Problem");

    httplib::Request req = createMultipartRequest("POST", "/api/problems/" + std::to_string(problemId) + "/testcases",
                                                  "session_token=" + adminToken);
    req.path_params["id"] = std::to_string(problemId);

    addFileToRequest(req, "input", "1.in", "1 2 3");
    addFileToRequest(req, "output", "1.out", "6");

    httplib::Response res;

    handleAddTestCase(req, res);

    EXPECT_EQ(201, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("Test case added successfully", json["message"].asString());
    EXPECT_GT(json["id"].asInt(), 0);

    int tcId = json["id"].asInt();
    auto tcOpt = TestCase::findById(tcId);
    EXPECT_TRUE(tcOpt.has_value());
    EXPECT_EQ(problemId, tcOpt->problem_id);
    EXPECT_EQ(100, tcOpt->score);
    EXPECT_FALSE(tcOpt->is_sample);
}

TEST_F(TestCaseHandlerDbTest, AddTestCase_AsSample) {
    int problemId = createProblem("Test Problem");

    httplib::Request req = createMultipartRequest("POST", "/api/problems/" + std::to_string(problemId) + "/testcases",
                                                  "session_token=" + adminToken);
    req.path_params["id"] = std::to_string(problemId);
    req.params.emplace("is_sample", "true");

    addFileToRequest(req, "input", "1.in", "1 2 3");
    addFileToRequest(req, "output", "1.out", "6");

    httplib::Response res;

    handleAddTestCase(req, res);

    EXPECT_EQ(201, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_TRUE(json["is_sample"].asBool());

    int tcId = json["id"].asInt();
    auto tcOpt = TestCase::findById(tcId);
    EXPECT_TRUE(tcOpt->is_sample);
}

TEST_F(TestCaseHandlerDbTest, AddTestCase_WithoutAuth) {
    int problemId = createProblem("Test Problem");

    httplib::Request req = createMultipartRequest("POST", "/api/problems/" + std::to_string(problemId) + "/testcases");
    req.path_params["id"] = std::to_string(problemId);

    addFileToRequest(req, "input", "1.in", "1 2 3");
    addFileToRequest(req, "output", "1.out", "6");

    httplib::Response res;

    handleAddTestCase(req, res);

    EXPECT_EQ(401, res.status);
}

TEST_F(TestCaseHandlerDbTest, AddTestCase_NonAdminUser) {
    int problemId = createProblem("Test Problem");

    httplib::Request req = createMultipartRequest("POST", "/api/problems/" + std::to_string(problemId) + "/testcases",
                                                  "session_token=" + userToken);
    req.path_params["id"] = std::to_string(problemId);

    addFileToRequest(req, "input", "1.in", "1 2 3");
    addFileToRequest(req, "output", "1.out", "6");

    httplib::Response res;

    handleAddTestCase(req, res);

    EXPECT_EQ(403, res.status);
}

TEST_F(TestCaseHandlerDbTest, AddTestCase_ProblemNotFound) {
    httplib::Request req = createMultipartRequest("POST", "/api/problems/99999/testcases",
                                                  "session_token=" + adminToken);
    req.path_params["id"] = "99999";

    addFileToRequest(req, "input", "1.in", "1 2 3");
    addFileToRequest(req, "output", "1.out", "6");

    httplib::Response res;

    handleAddTestCase(req, res);

    EXPECT_EQ(404, res.status);
}

TEST_F(TestCaseHandlerDbTest, AddTestCase_MissingInputFile) {
    int problemId = createProblem("Test Problem");

    httplib::Request req = createMultipartRequest("POST", "/api/problems/" + std::to_string(problemId) + "/testcases",
                                                  "session_token=" + adminToken);
    req.path_params["id"] = std::to_string(problemId);

    addFileToRequest(req, "output", "1.out", "6");

    httplib::Response res;

    handleAddTestCase(req, res);

    EXPECT_EQ(400, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("Both input and output files are required", json["error"].asString());
}

TEST_F(TestCaseHandlerDbTest, AddTestCase_MissingOutputFile) {
    int problemId = createProblem("Test Problem");

    httplib::Request req = createMultipartRequest("POST", "/api/problems/" + std::to_string(problemId) + "/testcases",
                                                  "session_token=" + adminToken);
    req.path_params["id"] = std::to_string(problemId);

    addFileToRequest(req, "input", "1.in", "1 2 3");

    httplib::Response res;

    handleAddTestCase(req, res);

    EXPECT_EQ(400, res.status);
}

TEST_F(TestCaseHandlerDbTest, DeleteTestCase_Success) {
    int problemId = createProblem("Test Problem");

    TestCase tc;
    tc.problem_id = problemId;
    tc.input_path = "/tmp/test_cases/" + std::to_string(problemId) + "/1_in.txt";
    tc.output_path = "/tmp/test_cases/" + std::to_string(problemId) + "/1_out.txt";
    tc.score = 100;
    tc.is_sample = false;
    tc.create();

    int tcId = tc.id;

    httplib::Request req = createRequest("DELETE", "/api/testcases/" + std::to_string(tcId),
                                        "", "session_token=" + adminToken);
    req.path_params["id"] = std::to_string(tcId);
    httplib::Response res;

    handleDeleteTestCase(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("Test case deleted successfully", json["message"].asString());

    auto tcOpt = TestCase::findById(tcId);
    EXPECT_FALSE(tcOpt.has_value());
}

TEST_F(TestCaseHandlerDbTest, DeleteTestCase_WithoutAuth) {
    int problemId = createProblem("Test Problem");

    TestCase tc;
    tc.problem_id = problemId;
    tc.input_path = "/tmp/test_cases/" + std::to_string(problemId) + "/1_in.txt";
    tc.output_path = "/tmp/test_cases/" + std::to_string(problemId) + "/1_out.txt";
    tc.score = 100;
    tc.is_sample = false;
    tc.create();

    int tcId = tc.id;

    httplib::Request req = createRequest("DELETE", "/api/testcases/" + std::to_string(tcId), "", "");
    req.path_params["id"] = std::to_string(tcId);
    httplib::Response res;

    handleDeleteTestCase(req, res);

    EXPECT_EQ(401, res.status);
}

TEST_F(TestCaseHandlerDbTest, DeleteTestCase_NonAdminUser) {
    int problemId = createProblem("Test Problem");

    TestCase tc;
    tc.problem_id = problemId;
    tc.input_path = "/tmp/test_cases/" + std::to_string(problemId) + "/1_in.txt";
    tc.output_path = "/tmp/test_cases/" + std::to_string(problemId) + "/1_out.txt";
    tc.score = 100;
    tc.is_sample = false;
    tc.create();

    int tcId = tc.id;

    httplib::Request req = createRequest("DELETE", "/api/testcases/" + std::to_string(tcId),
                                        "", "session_token=" + userToken);
    req.path_params["id"] = std::to_string(tcId);
    httplib::Response res;

    handleDeleteTestCase(req, res);

    EXPECT_EQ(403, res.status);
}

TEST_F(TestCaseHandlerDbTest, DeleteTestCase_NotFound) {
    httplib::Request req = createRequest("DELETE", "/api/testcases/99999",
                                        "", "session_token=" + adminToken);
    req.path_params["id"] = "99999";
    httplib::Response res;

    handleDeleteTestCase(req, res);

    EXPECT_EQ(404, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("Test case not found", json["error"].asString());
}