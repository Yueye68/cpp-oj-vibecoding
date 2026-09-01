#include "user.h"
#include "logger.h"
#include <cstring>
#include <sstream>

std::string User::roleToString(UserRole r) {
    return r == UserRole::Admin ? "admin" : "user";
}

UserRole User::stringToRole(const std::string& s) {
    return s == "admin" ? UserRole::Admin : UserRole::User;
}

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

bool User::create() {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for User::create");
        return false;
    }

    std::ostringstream query;
    query << "INSERT INTO users (username, password_hash, role) VALUES ("
          << escapeString(conn, username) << ", "
          << escapeString(conn, password_hash) << ", '"
          << roleToString(role) << "')";

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to create user: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    id = static_cast<int>(mysql_insert_id(conn));
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool User::update() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for User::update");
        return false;
    }

    std::ostringstream query;
    query << "UPDATE users SET username = " << escapeString(conn, username)
          << ", password_hash = " << escapeString(conn, password_hash)
          << ", role = '" << roleToString(role) << "' WHERE id = " << id;

    if (mysql_real_query(conn, query.str().c_str(), query.str().size()) != 0) {
        Logger::instance().error("Failed to update user: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool User::remove() {
    if (id <= 0) return false;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for User::remove");
        return false;
    }

    std::string query = "DELETE FROM users WHERE id = " + std::to_string(id);
    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to delete user: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return false;
    }

    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool User::loadFromDB(int loadId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for User::loadFromDB");
        return false;
    }

    std::string query = "SELECT id, username, password_hash, role, created_at FROM users WHERE id = " + std::to_string(loadId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to load user from DB: " + std::string(mysql_error(conn)));
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
    username = row[1] ? row[1] : "";
    password_hash = row[2] ? row[2] : "";
    role = stringToRole(row[3] ? row[3] : "user");
    created_at = row[4] ? row[4] : "";

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return true;
}

bool User::saveToDB() {
    if (id <= 0) {
        return create();
    } else {
        return update();
    }
}

std::optional<User> User::findById(int userId) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for User::findById");
        return std::nullopt;
    }

    std::string query = "SELECT id, username, password_hash, role, created_at FROM users WHERE id = " + std::to_string(userId);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find user by id: " + std::string(mysql_error(conn)));
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

    User user;
    user.id = std::stoi(row[0]);
    user.username = row[1] ? row[1] : "";
    user.password_hash = row[2] ? row[2] : "";
    user.role = stringToRole(row[3] ? row[3] : "user");
    user.created_at = row[4] ? row[4] : "";

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return user;
}

std::optional<User> User::findByUsername(const std::string& username) {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for User::findByUsername");
        return std::nullopt;
    }

    std::string query = "SELECT id, username, password_hash, role, created_at FROM users WHERE username = " + escapeString(conn, username);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find user by username: " + std::string(mysql_error(conn)));
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

    User user;
    user.id = std::stoi(row[0]);
    user.username = row[1] ? row[1] : "";
    user.password_hash = row[2] ? row[2] : "";
    user.role = stringToRole(row[3] ? row[3] : "user");
    user.created_at = row[4] ? row[4] : "";

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return user;
}

std::vector<User> User::findAll(int page, int pageSize) {
    std::vector<User> users;

    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for User::findAll");
        return users;
    }

    std::string query = "SELECT id, username, password_hash, role, created_at FROM users ORDER BY id DESC LIMIT " +
                       std::to_string((page - 1) * pageSize) + "," + std::to_string(pageSize);

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to find users: " + std::string(mysql_error(conn)));
        ConnectionPool::instance().returnConnection(conn);
        return users;
    }

    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) {
        ConnectionPool::instance().returnConnection(conn);
        return users;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        User user;
        user.id = std::stoi(row[0]);
        user.username = row[1] ? row[1] : "";
        user.password_hash = row[2] ? row[2] : "";
        user.role = stringToRole(row[3] ? row[3] : "user");
        user.created_at = row[4] ? row[4] : "";
        users.push_back(user);
    }

    mysql_free_result(result);
    ConnectionPool::instance().returnConnection(conn);
    return users;
}

int User::count() {
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (!conn) {
        Logger::instance().error("Failed to get database connection for User::count");
        return 0;
    }

    std::string query = "SELECT COUNT(*) FROM users";

    if (mysql_real_query(conn, query.c_str(), query.size()) != 0) {
        Logger::instance().error("Failed to count users: " + std::string(mysql_error(conn)));
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