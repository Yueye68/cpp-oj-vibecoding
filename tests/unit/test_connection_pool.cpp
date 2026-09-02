#include <gtest/gtest.h>
#include "db_pool/connection_pool.h"
#include "utils/config.h"

class ConnectionPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        ConnectionPool::instance().close();
    }

    void TearDown() override {
        ConnectionPool::instance().close();
    }
};

TEST_F(ConnectionPoolTest, Singleton_Persistence) {
    ConnectionPool& pool1 = ConnectionPool::instance();
    ConnectionPool& pool2 = ConnectionPool::instance();
    EXPECT_EQ(&pool1, &pool2);
}

TEST_F(ConnectionPoolTest, Init_ValidConfig) {
    DatabaseConfig dbConfig;
    dbConfig.host = "localhost";
    dbConfig.port = 3306;
    dbConfig.username = "root";
    dbConfig.password = "1";
    dbConfig.database = "oj_system";
    dbConfig.charset = "utf8mb4";

    ConnectionPool::instance().init(dbConfig, 5);

    EXPECT_EQ(5, ConnectionPool::instance().getPoolSize());
    EXPECT_TRUE(ConnectionPool::instance().getAvailableCount() >= 0);
}

TEST_F(ConnectionPoolTest, Init_DefaultPoolSize) {
    DatabaseConfig dbConfig;
    dbConfig.host = "localhost";
    dbConfig.port = 3306;
    dbConfig.username = "root";
    dbConfig.password = "1";
    dbConfig.database = "oj_system";

    ConnectionPool::instance().init(dbConfig);

    EXPECT_EQ(10, ConnectionPool::instance().getPoolSize());
}

TEST_F(ConnectionPoolTest, Init_CalledTwice) {
    DatabaseConfig dbConfig;
    dbConfig.host = "localhost";
    dbConfig.port = 3306;
    dbConfig.username = "root";
    dbConfig.password = "1";
    dbConfig.database = "oj_system";

    ConnectionPool::instance().init(dbConfig, 3);
    int firstPoolSize = ConnectionPool::instance().getPoolSize();

    ConnectionPool::instance().init(dbConfig, 7);
    int secondPoolSize = ConnectionPool::instance().getPoolSize();

    EXPECT_EQ(firstPoolSize, secondPoolSize);
}

TEST_F(ConnectionPoolTest, Close_EmptyPool) {
    ConnectionPool::instance().close();
    EXPECT_EQ(0, ConnectionPool::instance().getAvailableCount());
}

TEST_F(ConnectionPoolTest, GetAvailableCount_InitiallyZeroOrMore) {
    DatabaseConfig dbConfig;
    dbConfig.host = "localhost";
    dbConfig.port = 3306;
    dbConfig.username = "root";
    dbConfig.password = "1";
    dbConfig.database = "oj_system";

    ConnectionPool::instance().init(dbConfig, 3);

    int available = ConnectionPool::instance().getAvailableCount();
    EXPECT_GE(available, 0);
    EXPECT_LE(available, 3);
}

TEST_F(ConnectionPoolTest, GetConnection_BeforeInit) {
    ConnectionPool::instance().close();
    DatabaseConfig dbConfig;
    dbConfig.host = "localhost";
    dbConfig.port = 3306;
    dbConfig.username = "root";
    dbConfig.password = "1";
    dbConfig.database = "oj_system";
    ConnectionPool::instance().init(dbConfig, 1);
    MYSQL* conn = ConnectionPool::instance().getConnection();
    if (conn) {
        ConnectionPool::instance().returnConnection(conn);
    }
}

TEST_F(ConnectionPoolTest, ReturnConnection_Nullptr) {
    EXPECT_NO_THROW(ConnectionPool::instance().returnConnection(nullptr));
}

TEST_F(ConnectionPoolTest, PoolSize_AfterInit) {
    DatabaseConfig dbConfig;
    dbConfig.host = "localhost";
    dbConfig.port = 3306;
    dbConfig.username = "root";
    dbConfig.password = "1";
    dbConfig.database = "oj_system";

    ConnectionPool::instance().init(dbConfig, 7);
    EXPECT_EQ(7, ConnectionPool::instance().getPoolSize());
}
