#include "connection_pool.h"
#include "logger.h"
#include <stdexcept>

ConnectionPool& ConnectionPool::instance() {
    static ConnectionPool pool;
    return pool;
}

ConnectionPool::~ConnectionPool() {
    close();
}

void ConnectionPool::init(const DatabaseConfig& config, int poolSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return;
    }
    dbConfig_ = config;
    poolSize_ = poolSize;
    for (int i = 0; i < poolSize_; ++i) {
        MYSQL* conn = nullptr;
        if (createConnection(conn)) {
            connections_.push_back(std::make_unique<ConnectionWrapper>(conn));
            availableConnections_.push(conn);
        } else {
            Logger::instance().error("Failed to create connection " + std::to_string(i));
        }
    }
    initialized_ = true;
    Logger::instance().info("Connection pool initialized with " + std::to_string(poolSize_) + " connections");
}

bool ConnectionPool::createConnection(MYSQL*& conn) {
    conn = mysql_init(nullptr);
    if (!conn) {
        return false;
    }

    if (!mysql_real_connect(conn,
                           dbConfig_.host.c_str(),
                           dbConfig_.username.c_str(),
                           dbConfig_.password.c_str(),
                           dbConfig_.database.c_str(),
                           dbConfig_.port,
                           nullptr,
                           0)) {
        mysql_close(conn);
        return false;
    }

    mysql_set_character_set(conn, dbConfig_.charset.c_str());
    return true;
}

void ConnectionPool::destroyConnection(MYSQL* conn) {
    if (conn) {
        mysql_close(conn);
    }
}

MYSQL* ConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] {
        return !availableConnections_.empty();
    });

    MYSQL* conn = availableConnections_.front();
    availableConnections_.pop();

    if (!mysql_ping(conn)) {
        return conn;
    }

    mysql_close(conn);
    MYSQL* newConn = nullptr;
    if (createConnection(newConn)) {
        return newConn;
    }

    Logger::instance().error("Failed to reconnect, expanding pool");
    expandPool();
    if (!availableConnections_.empty()) {
        conn = availableConnections_.front();
        availableConnections_.pop();
        return conn;
    }
    return nullptr;
}

void ConnectionPool::returnConnection(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn && mysql_ping(conn) == 0) {
        availableConnections_.push(conn);
        cv_.notify_one();
    } else {
        if (conn) {
            mysql_close(conn);
        }
        MYSQL* newConn = nullptr;
        if (createConnection(newConn)) {
            availableConnections_.push(newConn);
            cv_.notify_one();
        }
    }
}

void ConnectionPool::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!availableConnections_.empty()) {
        availableConnections_.pop();
    }
    connections_.clear();
    initialized_ = false;
    Logger::instance().info("Connection pool closed");
}

int ConnectionPool::getAvailableCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(availableConnections_.size());
}

void ConnectionPool::expandPool() {
    MYSQL* conn = nullptr;
    if (createConnection(conn)) {
        connections_.push_back(std::make_unique<ConnectionWrapper>(conn));
        availableConnections_.push(conn);
        cv_.notify_one();
    }
}

void ConnectionPool::shrinkPool() {
    if (!availableConnections_.empty()) {
        MYSQL* conn = availableConnections_.front();
        availableConnections_.pop();
        destroyConnection(conn);
    }
}
