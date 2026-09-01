#include "submission_result.h"
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

bool SubmissionResult::create() {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for SubmissionResult::create");
        return false;
    }

    std::ostringstream query;
    query << "INSERT INTO submission_results (submission_id, test_case_id, status, actual_output, expected_output, execute_time_ms, execute_memory_kb) VALUES ("
          << submission_id << ", " << test_case_id << ", "
          << escapeString(conn, status) << ", "
          << escapeString(conn, actual_output) << ", "
          << escapeString(conn, expected_output) << ", "
          << execute_time_ms << ", " << execute_memory_kb << ")";

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to create submission result: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    id = static_cast<int>(mysql_insert_id(conn));
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool SubmissionResult::update() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for SubmissionResult::update");
        return false;
    }

    std::ostringstream query;
    query << "UPDATE submission_results SET status = " << escapeString(conn, status)
          << ", actual_output = " << escapeString(conn, actual_output)
          << ", expected_output = " << escapeString(conn, expected_output)
          << ", execute_time_ms = " << execute_time_ms
          << ", execute_memory_kb = " << execute_memory_kb
          << " WHERE id = " << id;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to update submission result: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool SubmissionResult::remove() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for SubmissionResult::remove");
        return false;
    }

    std::string query = "DELETE FROM submission_results WHERE id = " + std::to_string(id);
    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to delete submission result: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool SubmissionResult::loadFromDB(int loadId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for SubmissionResult::loadFromDB");
        return false;
    }

    std::string query = "SELECT id, submission_id, test_case_id, status, actual_output, expected_output, execute_time_ms, execute_memory_kb FROM submission_results WHERE id = " + std::to_string(loadId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to load submission result from DB: " + std::string(mysql_error(conn)));
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
    submission_id = std::stoi(row[1]);
    test_case_id = std::stoi(row[2]);
    status = row[3] ? row[3] : "";
    actual_output = row[4] ? row[4] : "";
    expected_output = row[5] ? row[5] : "";
    execute_time_ms = row[6] ? std::stoi(row[6]) : 0;
    execute_memory_kb = row[7] ? std::stoi(row[7]) : 0;

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool SubmissionResult::saveToDB() {
    if (id <= 0) {
        return create();
    } else {
        return update();
    }
}

std::optional<SubmissionResult> SubmissionResult::findById(int resultId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for SubmissionResult::findById");
        return std::nullopt;
    }

    std::string query = "SELECT id, submission_id, test_case_id, status, actual_output, expected_output, execute_time_ms, execute_memory_kb FROM submission_results WHERE id = " + std::to_string(resultId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find submission result by id: " + std::string(mysql_error(conn)));
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

    SubmissionResult sr;
    sr.id = std::stoi(row[0]);
    sr.submission_id = std::stoi(row[1]);
    sr.test_case_id = std::stoi(row[2]);
    sr.status = row[3] ? row[3] : "";
    sr.actual_output = row[4] ? row[4] : "";
    sr.expected_output = row[5] ? row[5] : "";
    sr.execute_time_ms = row[6] ? std::stoi(row[6]) : 0;
    sr.execute_memory_kb = row[7] ? std::stoi(row[7]) : 0;

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return sr;
}

std::vector<SubmissionResult> SubmissionResult::findBySubmissionId(int subId) {
    std::vector<SubmissionResult> results;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for SubmissionResult::findBySubmissionId");
        return results;
    }

    std::string query = "SELECT id, submission_id, test_case_id, status, actual_output, expected_output, execute_time_ms, execute_memory_kb FROM submission_results WHERE submission_id = " + std::to_string(subId) + " ORDER BY id";

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find submission results by submission id: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return results;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return results;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        SubmissionResult sr;
        sr.id = std::stoi(row[0]);
        sr.submission_id = std::stoi(row[1]);
        sr.test_case_id = std::stoi(row[2]);
        sr.status = row[3] ? row[3] : "";
        sr.actual_output = row[4] ? row[4] : "";
        sr.expected_output = row[5] ? row[5] : "";
        sr.execute_time_ms = row[6] ? std::stoi(row[6]) : 0;
        sr.execute_memory_kb = row[7] ? std::stoi(row[7]) : 0;
        results.push_back(sr);
    }

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return results;
}