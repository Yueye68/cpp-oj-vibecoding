#include "problem.h"
#include "logger.h"
#include <sstream>
#include <regex>

std::string Problem::difficultyToString(Difficulty d) {
    switch (d) {
        case Difficulty::Easy: return "easy";
        case Difficulty::Medium: return "medium";
        case Difficulty::Hard: return "hard";
        default: return "easy";
    }
}

Difficulty Problem::stringToDifficulty(const std::string& s) {
    if (s == "easy") return Difficulty::Easy;
    if (s == "medium") return Difficulty::Medium;
    if (s == "hard") return Difficulty::Hard;
    return Difficulty::Easy;
}

std::string Problem::tagsToJson(const std::vector<std::string>& tags) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << tags[i] << "\"";
    }
    oss << "]";
    return oss.str();
}

std::vector<std::string> Problem::jsonToTags(const std::string& json) {
    std::vector<std::string> result;
    if (json.empty() || json == "null") return result;

    std::regex tag_regex("\"([^\"]+)\"");
    for (std::sregex_iterator it(json.begin(), json.end(), tag_regex); it != std::sregex_iterator(); ++it) {
        result.push_back((*it)[1].str());
    }
    return result;
}

bool Problem::create() {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Problem::create");
        return false;
    }

    std::string tagsJson = tagsToJson(tags);
    std::string query = "INSERT INTO problems (title, description, difficulty, tags, time_limit_ms, memory_limit_mb) VALUES (?, ?, ?, ?, ?, ?)";

    mysql_real_query(conn, query.c_str(), query.size());

    const char* title_str = title.c_str();
    const char* desc_str = description.c_str();
    const char* diff_str = difficultyToString(difficulty).c_str();
    const char* tags_str = tagsJson.c_str();

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.size()) != 0) {
        mysql_stmt_close(stmt);
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    MYSQL_BIND bind[6];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)title_str;
    bind[0].buffer_length = title.size();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)desc_str;
    bind[1].buffer_length = description.size();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)diff_str;
    bind[2].buffer_length = difficultyToString(difficulty).size();

    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)tags_str;
    bind[3].buffer_length = tagsJson.size();

    bind[4].buffer_type = MYSQL_TYPE_LONG;
    bind[4].buffer = &time_limit_ms;

    bind[5].buffer_type = MYSQL_TYPE_LONG;
    bind[5].buffer = &memory_limit_mb;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        Logger::instance().error("Failed to create problem: " + std::string(mysql_stmt_error(stmt)));
        mysql_stmt_close(stmt);
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    id = mysql_insert_id(conn);
    mysql_stmt_close(stmt);
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool Problem::update() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Problem::update");
        return false;
    }

    std::string tagsJson = tagsToJson(tags);
    std::string query = "UPDATE problems SET title = ?, description = ?, difficulty = ?, tags = ?, time_limit_ms = ?, memory_limit_mb = ? WHERE id = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.size()) != 0) {
        mysql_stmt_close(stmt);
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    const char* title_str = title.c_str();
    const char* desc_str = description.c_str();
    const char* diff_str = difficultyToString(difficulty).c_str();
    const char* tags_str = tagsJson.c_str();

    MYSQL_BIND bind[7];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)title_str;
    bind[0].buffer_length = title.size();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)desc_str;
    bind[1].buffer_length = description.size();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)diff_str;
    bind[2].buffer_length = difficultyToString(difficulty).size();

    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)tags_str;
    bind[3].buffer_length = tagsJson.size();

    bind[4].buffer_type = MYSQL_TYPE_LONG;
    bind[4].buffer = &time_limit_ms;

    bind[5].buffer_type = MYSQL_TYPE_LONG;
    bind[5].buffer = &memory_limit_mb;

    bind[6].buffer_type = MYSQL_TYPE_LONG;
    bind[6].buffer = &id;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        Logger::instance().error("Failed to update problem: " + std::string(mysql_stmt_error(stmt)));
        mysql_stmt_close(stmt);
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    mysql_stmt_close(stmt);
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool Problem::remove() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Problem::remove");
        return false;
    }

    std::string query = "DELETE FROM problems WHERE id = " + std::to_string(id);
    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to delete problem: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool Problem::loadFromDB(int loadId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Problem::loadFromDB");
        return false;
    }

    std::string query = "SELECT id, title, description, difficulty, tags, time_limit_ms, memory_limit_mb, test_case_count, created_at, updated_at FROM problems WHERE id = " + std::to_string(loadId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to load problem from DB: " + std::string(mysql_error(conn)));
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
    title = row[1] ? row[1] : "";
    description = row[2] ? row[2] : "";
    difficulty = stringToDifficulty(row[3] ? row[3] : "easy");
    tags = jsonToTags(row[4] ? row[4] : "[]");
    time_limit_ms = row[5] ? std::stoi(row[5]) : 1000;
    memory_limit_mb = row[6] ? std::stoi(row[6]) : 256;
    test_case_count = row[7] ? std::stoi(row[7]) : 0;
    created_at = row[8] ? row[8] : "";
    updated_at = row[9] ? row[9] : "";

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool Problem::saveToDB() {
    if (id <= 0) {
        return create();
    } else {
        return update();
    }
}

std::optional<Problem> Problem::findById(int problemId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Problem::findById");
        return std::nullopt;
    }

    std::string query = "SELECT id, title, description, difficulty, tags, time_limit_ms, memory_limit_mb, test_case_count, created_at, updated_at FROM problems WHERE id = " + std::to_string(problemId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find problem by id: " + std::string(mysql_error(conn)));
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

    Problem problem;
    problem.id = std::stoi(row[0]);
    problem.title = row[1] ? row[1] : "";
    problem.description = row[2] ? row[2] : "";
    problem.difficulty = stringToDifficulty(row[3] ? row[3] : "easy");
    problem.tags = jsonToTags(row[4] ? row[4] : "[]");
    problem.time_limit_ms = row[5] ? std::stoi(row[5]) : 1000;
    problem.memory_limit_mb = row[6] ? std::stoi(row[6]) : 256;
    problem.test_case_count = row[7] ? std::stoi(row[7]) : 0;
    problem.created_at = row[8] ? row[8] : "";
    problem.updated_at = row[9] ? row[9] : "";

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return problem;
}

std::vector<Problem> Problem::findAll(int page, int pageSize, const std::string& difficulty, const std::vector<std::string>& tags, const std::string& search) {
    std::vector<Problem> problems;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Problem::findAll");
        return problems;
    }

    std::ostringstream query;
    query << "SELECT id, title, description, difficulty, tags, time_limit_ms, memory_limit_mb, test_case_count, created_at, updated_at FROM problems WHERE 1=1";

    if (!difficulty.empty()) {
        query << " AND difficulty = '" << difficulty << "'";
    }

    if (!search.empty()) {
        query << " AND title LIKE '%" << search << "%'";
    }

    if (!tags.empty()) {
        query << " AND (";
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) query << " OR ";
            query << "JSON_CONTAINS(tags, '\"' || '" << tags[i] << "' || '\"')";
        }
        query << ")";
    }

    query << " ORDER BY id DESC LIMIT " << (page - 1) * pageSize << "," << pageSize;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to find problems: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return problems;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return problems;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        Problem problem;
        problem.id = std::stoi(row[0]);
        problem.title = row[1] ? row[1] : "";
        problem.description = row[2] ? row[2] : "";
        problem.difficulty = stringToDifficulty(row[3] ? row[3] : "easy");
        problem.tags = jsonToTags(row[4] ? row[4] : "[]");
        problem.time_limit_ms = row[5] ? std::stoi(row[5]) : 1000;
        problem.memory_limit_mb = row[6] ? std::stoi(row[6]) : 256;
        problem.test_case_count = row[7] ? std::stoi(row[7]) : 0;
        problem.created_at = row[8] ? row[8] : "";
        problem.updated_at = row[9] ? row[9] : "";
        problems.push_back(problem);
    }

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return problems;
}

int Problem::count(const std::string& difficulty, const std::vector<std::string>& tags, const std::string& search) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for Problem::count");
        return 0;
    }

    std::ostringstream query;
    query << "SELECT COUNT(*) FROM problems WHERE 1=1";

    if (!difficulty.empty()) {
        query << " AND difficulty = '" << difficulty << "'";
    }

    if (!search.empty()) {
        query << " AND title LIKE '%" << search << "%'";
    }

    if (!tags.empty()) {
        query << " AND (";
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) query << " OR ";
            query << "JSON_CONTAINS(tags, '\"' || '" << tags[i] << "' || '\"')";
        }
        query << ")";
    }

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to count problems: " + std::string(mysql_error(conn)));
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