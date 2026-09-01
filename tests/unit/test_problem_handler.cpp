#include <gtest/gtest.h>
#include "server/handlers/problem_handler.h"
#include "server/middleware/auth_middleware.h"
#include "models/problem.h"
#include "models/user.h"
#include "db_pool/connection_pool.h"
#include "utils/config.h"
#include "utils/json.h"
#include <iostream>

extern std::string createSession(const User& user);
extern void destroySession(const std::string& token);

class ProblemHandlerTest : public ::testing::Test {
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

class ProblemHandlerDbTest : public ProblemHandlerTest {
protected:
    void SetUp() override {
        ProblemHandlerTest::SetUp();

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
        adminToken = createSession(adminUser);

        User normalUser;
        normalUser.id = userId;
        normalUser.username = "user_test";
        normalUser.role = UserRole::User;
        userToken = createSession(normalUser);
    }

    void TearDown() override {
        if (!adminToken.empty()) destroySession(adminToken);
        if (!userToken.empty()) destroySession(userToken);

        cleanTable("submission_results");
        cleanTable("submissions");
        cleanTable("test_cases");
        cleanTable("problems");
        cleanTable("users");

        ProblemHandlerTest::TearDown();
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
};

TEST_F(ProblemHandlerDbTest, ListProblems_Empty) {
    httplib::Request req = createRequest("GET", "/api/problems");
    httplib::Response res;

    handleListProblems(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    Json::String err;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, &err);

    EXPECT_TRUE(json["problems"].isArray());
    EXPECT_EQ(0, json["problems"].size());
    EXPECT_EQ(0, json["total"]);
    EXPECT_EQ(1, json["page"]);
}

TEST_F(ProblemHandlerDbTest, ListProblems_WithData) {
    createProblem("Two Sum", Difficulty::Easy, {"array", "hash-table"});
    createProblem("Add Two Numbers", Difficulty::Medium, {"linked-list", "math"});
    createProblem("Binary Search", Difficulty::Easy, {"binary-search"});

    httplib::Request req = createRequest("GET", "/api/problems");
    httplib::Response res;

    handleListProblems(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ(3, json["problems"].size());
    EXPECT_EQ(3, json["total"]);
}

TEST_F(ProblemHandlerDbTest, ListProblems_Pagination) {
    for (int i = 0; i < 25; ++i) {
        createProblem("Problem " + std::to_string(i), Difficulty::Easy);
    }

    httplib::Request req1 = createRequest("GET", "/api/problems?page=1&pageSize=10");
    httplib::Response res1;
    handleListProblems(req1, res1);

    Json::Value json1;
    std::istringstream iss1(res1.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss1, &json1, nullptr);

    EXPECT_EQ(10, json1["problems"].size());
    EXPECT_EQ(25, json1["total"]);
    EXPECT_EQ(1, json1["page"]);
    EXPECT_EQ(10, json1["pageSize"]);
    EXPECT_EQ(3, json1["totalPages"]);

    httplib::Request req2 = createRequest("GET", "/api/problems?page=2&pageSize=10");
    httplib::Response res2;
    handleListProblems(req2, res2);

    Json::Value json2;
    std::istringstream iss2(res2.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss2, &json2, nullptr);

    EXPECT_EQ(10, json2["problems"].size());
    EXPECT_EQ(2, json2["page"]);
}

TEST_F(ProblemHandlerDbTest, ListProblems_FilterByDifficulty) {
    createProblem("Easy Problem 1", Difficulty::Easy);
    createProblem("Easy Problem 2", Difficulty::Easy);
    createProblem("Medium Problem", Difficulty::Medium);
    createProblem("Hard Problem", Difficulty::Hard);

    httplib::Request req = createRequest("GET", "/api/problems?difficulty=easy");
    httplib::Response res;
    handleListProblems(req, res);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ(2, json["problems"].size());
    EXPECT_EQ(2, json["total"]);
}

TEST_F(ProblemHandlerDbTest, ListProblems_FilterBySearch) {
    createProblem("Two Sum", Difficulty::Easy);
    createProblem("Three Sum", Difficulty::Medium);
    createProblem("Four Sum", Difficulty::Hard);

    httplib::Request req = createRequest("GET", "/api/problems?search=Sum");
    httplib::Response res;
    handleListProblems(req, res);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ(3, json["problems"].size());
    EXPECT_EQ(3, json["total"]);
}

TEST_F(ProblemHandlerDbTest, ListProblems_SearchNoResults) {
    createProblem("Two Sum", Difficulty::Easy);

    httplib::Request req = createRequest("GET", "/api/problems?search=xyz123");
    httplib::Response res;
    handleListProblems(req, res);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ(0, json["problems"].size());
    EXPECT_EQ(0, json["total"]);
}

TEST_F(ProblemHandlerDbTest, GetProblem_Success) {
    int problemId = createProblem("Two Sum", Difficulty::Easy, {"array", "hash-table"});

    httplib::Request req = createRequest("GET", "/api/problems/" + std::to_string(problemId));
    req.path_params["id"] = std::to_string(problemId);
    httplib::Response res;

    handleGetProblem(req, res);

    EXPECT_EQ(200, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("Two Sum", json["title"].asString());
    EXPECT_EQ("Test description for Two Sum", json["description"].asString());
    EXPECT_EQ("easy", json["difficulty"].asString());
    EXPECT_EQ(2, json["tags"].size());
    EXPECT_EQ(1000, json["time_limit_ms"].asInt());
    EXPECT_EQ(256, json["memory_limit_mb"].asInt());
}

TEST_F(ProblemHandlerDbTest, GetProblem_NotFound) {
    httplib::Request req = createRequest("GET", "/api/problems/99999");
    req.path_params["id"] = "99999";
    httplib::Response res;

    handleGetProblem(req, res);

    EXPECT_EQ(404, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("Problem not found", json["error"].asString());
}

TEST_F(ProblemHandlerDbTest, CreateProblem_Success) {
    std::string body = R"({
        "title": "New Problem",
        "description": "New description",
        "difficulty": "medium",
        "tags": ["dp", "array"],
        "time_limit_ms": 2000,
        "memory_limit_mb": 512
    })";

    httplib::Request req = createRequest("POST", "/api/problems", body,
                                         "session_token=" + adminToken);
    httplib::Response res;

    handleCreateProblem(req, res);

    EXPECT_EQ(201, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("Problem created successfully", json["message"].asString());
    EXPECT_GT(json["id"].asInt(), 0);

    Problem loaded;
    EXPECT_TRUE(loaded.loadFromDB(json["id"].asInt()));
    EXPECT_EQ("New Problem", loaded.title);
    EXPECT_EQ("medium", Problem::difficultyToString(loaded.difficulty));
}

TEST_F(ProblemHandlerDbTest, CreateProblem_WithoutAuth) {
    std::string body = R"({"title": "New Problem", "difficulty": "easy"})";

    httplib::Request req = createRequest("POST", "/api/problems", body, "");
    httplib::Response res;

    handleCreateProblem(req, res);

    EXPECT_EQ(401, res.status);
}

TEST_F(ProblemHandlerDbTest, CreateProblem_NonAdminUser) {
    std::string body = R"({"title": "New Problem", "difficulty": "easy"})";

    httplib::Request req = createRequest("POST", "/api/problems", body,
                                         "session_token=" + userToken);
    httplib::Response res;

    handleCreateProblem(req, res);

    EXPECT_EQ(403, res.status);

    Json::Value json;
    std::istringstream iss(res.body);
    Json::parseFromStream(Json::CharReaderBuilder(), iss, &json, nullptr);

    EXPECT_EQ("Forbidden: admin access required", json["error"].asString());
}

TEST_F(ProblemHandlerDbTest, CreateProblem_InvalidJson) {
    httplib::Request req = createRequest("POST", "/api/problems", "invalid json",
                                         "session_token=" + adminToken);
    httplib::Response res;

    handleCreateProblem(req, res);

    EXPECT_EQ(400, res.status);
}

TEST_F(ProblemHandlerDbTest, CreateProblem_EmptyTitle) {
    std::string body = R"({"title": "", "difficulty": "easy"})";

    httplib::Request req = createRequest("POST", "/api/problems", body,
                                         "session_token=" + adminToken);
    httplib::Response res;

    handleCreateProblem(req, res);

    EXPECT_EQ(400, res.status);
}

TEST_F(ProblemHandlerDbTest, UpdateProblem_Success) {
    int problemId = createProblem("Original Title", Difficulty::Easy);

    std::string body = R"({
        "title": "Updated Title",
        "difficulty": "hard",
        "time_limit_ms": 3000
    })";

    httplib::Request req = createRequest("PUT", "/api/problems/" + std::to_string(problemId), body,
                                         "session_token=" + adminToken);
    req.path_params["id"] = std::to_string(problemId);
    httplib::Response res;

    handleUpdateProblem(req, res);

    EXPECT_EQ(200, res.status);

    Problem loaded;
    EXPECT_TRUE(loaded.loadFromDB(problemId));
    EXPECT_EQ("Updated Title", loaded.title);
    EXPECT_EQ(Difficulty::Hard, loaded.difficulty);
    EXPECT_EQ(3000, loaded.time_limit_ms);
}

TEST_F(ProblemHandlerDbTest, UpdateProblem_PartialUpdate) {
    int problemId = createProblem("Original", Difficulty::Easy);
    Problem original;
    original.loadFromDB(problemId);
    int originalTestCaseCount = original.test_case_count;

    std::string body = R"({"title": "New Title Only"})";

    httplib::Request req = createRequest("PUT", "/api/problems/" + std::to_string(problemId), body,
                                         "session_token=" + adminToken);
    req.path_params["id"] = std::to_string(problemId);
    httplib::Response res;

    handleUpdateProblem(req, res);

    EXPECT_EQ(200, res.status);

    Problem loaded;
    loaded.loadFromDB(problemId);
    EXPECT_EQ("New Title Only", loaded.title);
    EXPECT_EQ(originalTestCaseCount, loaded.test_case_count);
}

TEST_F(ProblemHandlerDbTest, UpdateProblem_NotFound) {
    std::string body = R"({"title": "Updated"})";

    httplib::Request req = createRequest("PUT", "/api/problems/99999", body,
                                         "session_token=" + adminToken);
    req.path_params["id"] = "99999";
    httplib::Response res;

    handleUpdateProblem(req, res);

    EXPECT_EQ(404, res.status);
}

TEST_F(ProblemHandlerDbTest, UpdateProblem_WithoutAuth) {
    std::string body = R"({"title": "Updated"})";

    httplib::Request req = createRequest("PUT", "/api/problems/1", body, "");
    httplib::Response res;

    handleUpdateProblem(req, res);

    EXPECT_EQ(401, res.status);
}

TEST_F(ProblemHandlerDbTest, DeleteProblem_Success) {
    int problemId = createProblem("To Delete", Difficulty::Easy);

    httplib::Request req = createRequest("DELETE", "/api/problems/" + std::to_string(problemId), "",
                                         "session_token=" + adminToken);
    req.path_params["id"] = std::to_string(problemId);
    httplib::Response res;

    handleDeleteProblem(req, res);

    EXPECT_EQ(200, res.status);

    Problem loaded;
    EXPECT_FALSE(loaded.loadFromDB(problemId));
}

TEST_F(ProblemHandlerDbTest, DeleteProblem_NotFound) {
    httplib::Request req = createRequest("DELETE", "/api/problems/99999", "",
                                         "session_token=" + adminToken);
    req.path_params["id"] = "99999";
    httplib::Response res;

    handleDeleteProblem(req, res);

    EXPECT_EQ(404, res.status);
}

TEST_F(ProblemHandlerDbTest, DeleteProblem_WithoutAuth) {
    httplib::Request req = createRequest("DELETE", "/api/problems/1", "", "");
    httplib::Response res;

    handleDeleteProblem(req, res);

    EXPECT_EQ(401, res.status);
}

TEST_F(ProblemHandlerDbTest, DeleteProblem_NonAdminUser) {
    int problemId = createProblem("To Delete", Difficulty::Easy);

    httplib::Request req = createRequest("DELETE", "/api/problems/" + std::to_string(problemId), "",
                                         "session_token=" + userToken);
    req.path_params["id"] = std::to_string(problemId);
    httplib::Response res;

    handleDeleteProblem(req, res);

    EXPECT_EQ(403, res.status);
}
