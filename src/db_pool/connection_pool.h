#pragma once

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>
#include "config.h"

class ConnectionPool {
public:
    static ConnectionPool& instance();

    ~ConnectionPool();

    void init(const DatabaseConfig& config, int poolSize = 10);

    MYSQL* getConnection();
    void returnConnection(MYSQL* conn);
    void close();

    int getPoolSize() const { return poolSize_; }
    int getAvailableCount() const;

private:
    ConnectionPool() = default;
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    bool createConnection(MYSQL*& conn);
    void destroyConnection(MYSQL* conn);
    void expandPool();
    void shrinkPool();

    struct ConnectionWrapper {
        MYSQL* conn;
        bool inUse;
        ConnectionWrapper(MYSQL* c) : conn(c), inUse(false) {}
    };

    std::vector<std::unique_ptr<ConnectionWrapper>> connections_;
    std::queue<MYSQL*> availableConnections_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    int poolSize_ = 10;
    DatabaseConfig dbConfig_;
    bool initialized_ = false;
};
