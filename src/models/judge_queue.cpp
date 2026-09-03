#include "judge_queue.h"
#include "logger.h"
#include <cstring>
#include <sstream>
#include <chrono>

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

bool JudgeQueueItem::create() {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for JudgeQueueItem::create");
        return false;
    }

    std::ostringstream query;
    query << "INSERT INTO judge_queue (submission_id, priority, status) VALUES ("
          << submission_id << ", " << priority << ", 'pending')";

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to create judge queue item: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    id = static_cast<int>(mysql_insert_id(conn));
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool JudgeQueueItem::update() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for JudgeQueueItem::update");
        return false;
    }

    std::ostringstream query;
    query << "UPDATE judge_queue SET status = " << escapeString(conn, status)
          << ", worker_id = " << (worker_id.empty() ? "NULL" : escapeString(conn, worker_id))
          << ", retry_count = " << retry_count
          << ", error_message = " << escapeString(conn, error_message)
          << " WHERE id = " << id;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to update judge queue item: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool JudgeQueueItem::markRunning(const std::string& wId) {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for JudgeQueueItem::markRunning");
        return false;
    }

    std::ostringstream query;
    query << "UPDATE judge_queue SET status = 'running', worker_id = " << escapeString(conn, wId)
          << ", started_at = NOW() WHERE id = " << id;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to mark running: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    status = "running";
    worker_id = wId;
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool JudgeQueueItem::markCompleted() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for JudgeQueueItem::markCompleted");
        return false;
    }

    std::ostringstream query;
    query << "UPDATE judge_queue SET status = 'completed', completed_at = NOW() WHERE id = " << id;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to mark completed: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    status = "completed";
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool JudgeQueueItem::markFailed(const std::string& error) {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for JudgeQueueItem::markFailed");
        return false;
    }

    std::ostringstream query;
    query << "UPDATE judge_queue SET status = 'failed', error_message = " << escapeString(conn, error)
          << ", completed_at = NOW() WHERE id = " << id;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to mark failed: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    status = "failed";
    error_message = error;
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

std::optional<JudgeQueueItem> JudgeQueueItem::popPending(const std::string& workerId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for JudgeQueueItem::popPending");
        return std::nullopt;
    }

    mysql_autocommit(conn, 0);

    std::string selectQuery = "SELECT id, submission_id, priority, status, worker_id, retry_count, "
                              "error_message, created_at, started_at, completed_at FROM judge_queue "
                              "WHERE status = 'pending' ORDER BY priority ASC, id ASC LIMIT 1 FOR UPDATE";

    if (mysql_real_query(conn, selectQuery.c_str(), selectQuery.size()) != 0) {
        Logger::instance().error("Failed to pop pending: " + std::string(mysql_error(conn)));
        mysql_rollback(conn);
        mysql_autocommit(conn, 1);
        ConnectionPool::instance().returnConnection(conn);
        return std::nullopt;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        mysql_rollback(conn);
        mysql_autocommit(conn, 1);
        ConnectionPool::instance().returnConnection(conn);
        return std::nullopt;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        mysql_rollback(conn);
        mysql_autocommit(conn, 1);
        ConnectionPool::instance().returnConnection(conn);
        return std::nullopt;
    }

    JudgeQueueItem item;
    item.id = std::stoi(row[0]);
    item.submission_id = std::stoi(row[1]);
    item.priority = row[2] ? std::stoi(row[2]) : 0;
    item.status = row[3] ? row[3] : "pending";
    item.worker_id = row[4] ? row[4] : "";
    item.retry_count = row[5] ? std::stoi(row[5]) : 0;
    item.error_message = row[6] ? row[6] : "";
    item.created_at = row[7] ? row[7] : "";
    item.started_at = row[8] ? row[8] : "";
    item.completed_at = row[9] ? row[9] : "";

    mysql_free_result(result);

    std::string updateQuery = "UPDATE judge_queue SET status = 'running', worker_id = " +
                              escapeString(conn, workerId) + ", started_at = NOW() WHERE id = " +
                              std::to_string(item.id);

    if (mysql_real_query(conn, updateQuery.c_str(), updateQuery.size()) != 0) {
        Logger::instance().error("Failed to update popped item: " + std::string(mysql_error(conn)));
        mysql_rollback(conn);
        mysql_autocommit(conn, 1);
        ConnectionPool::instance().returnConnection(conn);
        return std::nullopt;
    }

    if (mysql_commit(conn) != 0) {
        Logger::instance().error("Failed to commit pop: " + std::string(mysql_error(conn)));
        mysql_rollback(conn);
        mysql_autocommit(conn, 1);
        ConnectionPool::instance().returnConnection(conn);
        return std::nullopt;
    }

    mysql_autocommit(conn, 1);
    item.status = "running";
    item.worker_id = workerId;
    ConnectionPool::instance().returnConnection(conn);
    return item;
}

std::vector<JudgeQueueItem> JudgeQueueItem::findBySubmissionId(int subId) {
    std::vector<JudgeQueueItem> items;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for JudgeQueueItem::findBySubmissionId");
        return items;
    }

    std::string query = "SELECT id, submission_id, priority, status, worker_id, retry_count, "
                        "error_message, created_at, started_at, completed_at FROM judge_queue "
                        "WHERE submission_id = " + std::to_string(subId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find by submission id: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return items;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return items;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        JudgeQueueItem item;
        item.id = std::stoi(row[0]);
        item.submission_id = std::stoi(row[1]);
        item.priority = row[2] ? std::stoi(row[2]) : 0;
        item.status = row[3] ? row[3] : "pending";
        item.worker_id = row[4] ? row[4] : "";
        item.retry_count = row[5] ? std::stoi(row[5]) : 0;
        item.error_message = row[6] ? row[6] : "";
        item.created_at = row[7] ? row[7] : "";
        item.started_at = row[8] ? row[8] : "";
        item.completed_at = row[9] ? row[9] : "";
        items.push_back(item);
    }

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return items;
}

int JudgeQueueItem::countPending() {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for JudgeQueueItem::countPending");
        return 0;
    }

    std::string query = "SELECT COUNT(*) FROM judge_queue WHERE status = 'pending'";

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to count pending: " + std::string(mysql_error(conn)));
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