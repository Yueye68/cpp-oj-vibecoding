#include "test_case.h"
#include "logger.h"
#include <cstring>
#include <sstream>

static std::string escapeString(MYSQL* conn, const std::string& s) {
    std::string result;
    result.reserve(s.size() * 2 + 1);
    result.push_back('\'');
    for (char c : s) {
        if (c == '\'') result.push_back('\\');
        result.push_back(c);
    }
    result.push_back('\'');
    return result;
}

bool TestCase::create() {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for TestCase::create");
        return false;
    }

    std::ostringstream query;
    query << "INSERT INTO test_cases (problem_id, input_path, output_path, score, is_sample) VALUES ("
          << problem_id << ", "
          << escapeString(conn, input_path) << ", "
          << escapeString(conn, output_path) << ", "
          << score << ", " << (is_sample ? 1 : 0) << ")";

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to create test case: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    id = static_cast<int>(mysql_insert_id(conn));
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool TestCase::update() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for TestCase::update");
        return false;
    }

    std::ostringstream query;
    query << "UPDATE test_cases SET problem_id = " << problem_id
          << ", input_path = " << escapeString(conn, input_path)
          << ", output_path = " << escapeString(conn, output_path)
          << ", score = " << score
          << ", is_sample = " << (is_sample ? 1 : 0)
          << " WHERE id = " << id;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to update test case: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool TestCase::remove() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for TestCase::remove");
        return false;
    }

    std::string query = "DELETE FROM test_cases WHERE id = " + std::to_string(id);
    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to delete test case: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool TestCase::loadFromDB(int loadId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for TestCase::loadFromDB");
        return false;
    }

    std::string query = "SELECT id, problem_id, input_path, output_path, score, is_sample FROM test_cases WHERE id = " + std::to_string(loadId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to load test case from DB: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    id = std::stoi(row[0]);
    problem_id = std::stoi(row[1]);
    input_path = row[2] ? row[2] : "";
    output_path = row[3] ? row[3] : "";
    score = row[4] ? std::stoi(row[4]) : 100;
    is_sample = (row[5] && std::stoi(row[5]) == 1);

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool TestCase::saveToDB() {
    if (id <= 0) {
        return create();
    } else {
        return update();
    }
}

std::optional<TestCase> TestCase::findById(int testCaseId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for TestCase::findById");
        return std::nullopt;
    }

    std::string query = "SELECT id, problem_id, input_path, output_path, score, is_sample FROM test_cases WHERE id = " + std::to_string(testCaseId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find test case by id: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return std::nullopt;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return std::nullopt;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        ConnectionPool::instance().returnConnection(conn);
        return std::nullopt;
    }

    TestCase tc;
    tc.id = std::stoi(row[0]);
    tc.problem_id = std::stoi(row[1]);
    tc.input_path = row[2] ? row[2] : "";
    tc.output_path = row[3] ? row[3] : "";
    tc.score = row[4] ? std::stoi(row[4]) : 100;
    tc.is_sample = (row[5] && std::stoi(row[5]) == 1);

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return tc;
}

std::vector<TestCase> TestCase::findByProblemId(int problemId) {
    std::vector<TestCase> testCases;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for TestCase::findByProblemId");
        return testCases;
    }

    std::string query = "SELECT id, problem_id, input_path, output_path, score, is_sample FROM test_cases WHERE problem_id = " + std::to_string(problemId) + " ORDER BY id";

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find test cases by problem id: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return testCases;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return testCases;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        TestCase tc;
        tc.id = std::stoi(row[0]);
        tc.problem_id = std::stoi(row[1]);
        tc.input_path = row[2] ? row[2] : "";
        tc.output_path = row[3] ? row[3] : "";
        tc.score = row[4] ? std::stoi(row[4]) : 100;
        tc.is_sample = (row[5] && std::stoi(row[5]) == 1);
        testCases.push_back(tc);
    }

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return testCases;
}

int TestCase::countByProblemId(int problemId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for TestCase::countByProblemId");
        return 0;
    }

    std::string query = "SELECT COUNT(*) FROM test_cases WHERE problem_id = " + std::to_string(problemId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to count test cases: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return 0;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return count;
}