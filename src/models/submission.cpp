#include "submission.h"
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

bool Submission::create() {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::create");
        return false;
    }

    std::ostringstream query;
    query << "INSERT INTO submissions (user_id, problem_id, code, language, status, queue_status) VALUES ("
          << user_id << ", " << problem_id << ", "
          << escapeString(conn, code) << ", "
          << escapeString(conn, language) << ", "
          << escapeString(conn, status) << ", "
          << escapeString(conn, queue_status) << ")";

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to create submission: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    id = static_cast<int>(mysql_insert_id(conn));
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool Submission::update() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::update");
        return false;
    }

    std::ostringstream query;
    query << "UPDATE submissions SET status = " << escapeString(conn, status)
          << ", queue_status = " << escapeString(conn, queue_status)
          << ", error_detail = " << escapeString(conn, error_detail)
          << ", execute_time_ms = " << execute_time_ms
          << ", execute_memory_kb = " << execute_memory_kb
          << " WHERE id = " << id;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to update submission: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool Submission::updateQueueStatus(const std::string& qs) {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::updateQueueStatus");
        return false;
    }

    std::ostringstream query;
    query << "UPDATE submissions SET queue_status = " << escapeString(conn, qs)
          << " WHERE id = " << id;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to update queue status: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool Submission::remove() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::remove");
        return false;
    }

    std::string query = "DELETE FROM submissions WHERE id = " + std::to_string(id);
    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to delete submission: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool Submission::loadFromDB(int loadId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::loadFromDB");
        return false;
    }

    std::string query = "SELECT id, user_id, problem_id, code, language, status, queue_status, error_detail, execute_time_ms, execute_memory_kb, created_at FROM submissions WHERE id = " + std::to_string(loadId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to load submission from DB: " + std::string(mysql_error(conn)));
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
    user_id = std::stoi(row[1]);
    problem_id = std::stoi(row[2]);
    code = row[3] ? row[3] : "";
    language = row[4] ? row[4] : "cpp";
    status = row[5] ? row[5] : "";
    queue_status = row[6] ? row[6] : "pending";
    error_detail = row[7] ? row[7] : "";
    execute_time_ms = row[8] ? std::stoi(row[8]) : 0;
    execute_memory_kb = row[9] ? std::stoi(row[9]) : 0;
    created_at = row[10] ? row[10] : "";

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool Submission::saveToDB() {
    if (id <= 0) {
        return create();
    } else {
        return update();
    }
}

std::optional<Submission> Submission::findById(int submissionId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::findById");
        return std::nullopt;
    }

    std::string query = "SELECT id, user_id, problem_id, code, language, status, queue_status, error_detail, execute_time_ms, execute_memory_kb, created_at FROM submissions WHERE id = " + std::to_string(submissionId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find submission by id: " + std::string(mysql_error(conn)));
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

    Submission sub;
    sub.id = std::stoi(row[0]);
    sub.user_id = std::stoi(row[1]);
    sub.problem_id = std::stoi(row[2]);
    sub.code = row[3] ? row[3] : "";
    sub.language = row[4] ? row[4] : "cpp";
    sub.status = row[5] ? row[5] : "";
    sub.queue_status = row[6] ? row[6] : "pending";
    sub.error_detail = row[7] ? row[7] : "";
    sub.execute_time_ms = row[8] ? std::stoi(row[8]) : 0;
    sub.execute_memory_kb = row[9] ? std::stoi(row[9]) : 0;
    sub.created_at = row[10] ? row[10] : "";

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return sub;
}

std::vector<Submission> Submission::findAll(int page, int pageSize) {
    return findAll("", page, pageSize);
}

std::vector<Submission> Submission::findAll(const std::string& status, int page, int pageSize) {
    std::vector<Submission> submissions;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::findAll");
        return submissions;
    }

    std::string query = "SELECT s.id, s.user_id, s.problem_id, s.code, s.language, s.status, s.queue_status, s.error_detail, s.execute_time_ms, s.execute_memory_kb, s.created_at, p.title FROM submissions s LEFT JOIN problems p ON s.problem_id = p.id";
    if (!status.empty()) {
        query += " WHERE s.status = " + escapeString(conn, status);
    }
    query += " ORDER BY s.id DESC LIMIT " + std::to_string((page - 1) * pageSize) + "," + std::to_string(pageSize);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find all submissions: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return submissions;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return submissions;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        Submission sub;
        sub.id = std::stoi(row[0]);
        sub.user_id = std::stoi(row[1]);
        sub.problem_id = std::stoi(row[2]);
        sub.code = row[3] ? row[3] : "";
        sub.language = row[4] ? row[4] : "cpp";
        sub.status = row[5] ? row[5] : "";
        sub.queue_status = row[6] ? row[6] : "pending";
        sub.error_detail = row[7] ? row[7] : "";
        sub.execute_time_ms = row[8] ? std::stoi(row[8]) : 0;
        sub.execute_memory_kb = row[9] ? std::stoi(row[9]) : 0;
        sub.created_at = row[10] ? row[10] : "";
        sub.problem_title = row[11] ? row[11] : "";
        submissions.push_back(sub);
    }

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return submissions;
}

std::vector<Submission> Submission::findByUserId(int userId, int page, int pageSize) {
    return findByUserId(userId, "", page, pageSize);
}

std::vector<Submission> Submission::findByUserId(int userId, const std::string& status, int page, int pageSize) {
    std::vector<Submission> submissions;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::findByUserId");
        return submissions;
    }

    std::string query = "SELECT s.id, s.user_id, s.problem_id, s.code, s.language, s.status, s.queue_status, s.error_detail, s.execute_time_ms, s.execute_memory_kb, s.created_at, p.title FROM submissions s LEFT JOIN problems p ON s.problem_id = p.id WHERE s.user_id = " +
                       std::to_string(userId);
    if (!status.empty()) {
        query += " AND s.status = " + escapeString(conn, status);
    }
    query += " ORDER BY s.id DESC LIMIT " + std::to_string((page - 1) * pageSize) + "," + std::to_string(pageSize);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find submissions by user id: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return submissions;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return submissions;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        Submission sub;
        sub.id = std::stoi(row[0]);
        sub.user_id = std::stoi(row[1]);
        sub.problem_id = std::stoi(row[2]);
        sub.code = row[3] ? row[3] : "";
        sub.language = row[4] ? row[4] : "cpp";
        sub.status = row[5] ? row[5] : "";
        sub.queue_status = row[6] ? row[6] : "pending";
        sub.error_detail = row[7] ? row[7] : "";
        sub.execute_time_ms = row[8] ? std::stoi(row[8]) : 0;
        sub.execute_memory_kb = row[9] ? std::stoi(row[9]) : 0;
        sub.created_at = row[10] ? row[10] : "";
        sub.problem_title = row[11] ? row[11] : "";
        submissions.push_back(sub);
    }

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return submissions;
}

std::vector<Submission> Submission::findByProblemId(int problemId, int page, int pageSize) {
    std::vector<Submission> submissions;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::findByProblemId");
        return submissions;
    }

    std::string query = "SELECT id, user_id, problem_id, code, language, status, queue_status, error_detail, execute_time_ms, execute_memory_kb, created_at FROM submissions WHERE problem_id = " +
                       std::to_string(problemId) + " ORDER BY id DESC LIMIT " + std::to_string((page - 1) * pageSize) + "," + std::to_string(pageSize);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find submissions by problem id: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return submissions;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return submissions;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        Submission sub;
        sub.id = std::stoi(row[0]);
        sub.user_id = std::stoi(row[1]);
        sub.problem_id = std::stoi(row[2]);
        sub.code = row[3] ? row[3] : "";
        sub.language = row[4] ? row[4] : "cpp";
        sub.status = row[5] ? row[5] : "";
        sub.queue_status = row[6] ? row[6] : "pending";
        sub.error_detail = row[7] ? row[7] : "";
        sub.execute_time_ms = row[8] ? std::stoi(row[8]) : 0;
        sub.execute_memory_kb = row[9] ? std::stoi(row[9]) : 0;
        sub.created_at = row[10] ? row[10] : "";
        submissions.push_back(sub);
    }

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return submissions;
}

int Submission::countAll() {
    return countAll("");
}

int Submission::countAll(const std::string& status) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::countAll");
        return 0;
    }

    std::string query = "SELECT COUNT(*) FROM submissions";
    if (!status.empty()) {
        query += " WHERE status = " + escapeString(conn, status);
    }

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to count all submissions: " + std::string(mysql_error(conn)));
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

int Submission::countByUserId(int userId) {
    return countByUserId(userId, "");
}

int Submission::countByUserId(int userId, const std::string& status) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::countByUserId");
        return 0;
    }

    std::string query = "SELECT COUNT(*) FROM submissions WHERE user_id = " + std::to_string(userId);
    if (!status.empty()) {
        query += " AND status = " + escapeString(conn, status);
    }

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to count submissions by user id: " + std::string(mysql_error(conn)));
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

int Submission::countByProblemId(int problemId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Submission::countByProblemId");
        return 0;
    }

    std::string query = "SELECT COUNT(*) FROM submissions WHERE problem_id = " + std::to_string(problemId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to count submissions by problem id: " + std::string(mysql_error(conn)));
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