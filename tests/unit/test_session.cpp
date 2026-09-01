#include <gtest/gtest.h>
#include "utils/session.h"
#include "models/user.h"
#include <thread>
#include <vector>
#include <chrono>

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        SessionManager::instance().clearAllSessions();
    }

    void TearDown() override {
        SessionManager::instance().clearAllSessions();
    }

    User createTestUser(const std::string& username, UserRole role = UserRole::User) {
        User user;
        user.id = 0;
        user.username = username;
        user.password_hash = "test_hash";
        user.role = role;
        user.created_at = "2024-01-01 00:00:00";
        return user;
    }
};

TEST_F(SessionManagerTest, Singleton_Persistence) {
    SessionManager& mgr1 = SessionManager::instance();
    SessionManager& mgr2 = SessionManager::instance();
    EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(SessionManagerTest, CreateSession_Basic) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    EXPECT_FALSE(token.empty());
    EXPECT_TRUE(SessionManager::instance().validateSession(token));
}

TEST_F(SessionManagerTest, CreateSession_MultipleSessions) {
    User user = createTestUser("testuser");
    std::string token1 = SessionManager::instance().createSession(user);
    std::string token2 = SessionManager::instance().createSession(user);

    EXPECT_FALSE(token1.empty());
    EXPECT_FALSE(token2.empty());
    EXPECT_NE(token1, token2);
    EXPECT_TRUE(SessionManager::instance().validateSession(token1));
    EXPECT_TRUE(SessionManager::instance().validateSession(token2));
}

TEST_F(SessionManagerTest, CreateSession_DifferentUsers) {
    User user1 = createTestUser("user1");
    User user2 = createTestUser("user2");
    std::string token1 = SessionManager::instance().createSession(user1);
    std::string token2 = SessionManager::instance().createSession(user2);

    EXPECT_FALSE(token1.empty());
    EXPECT_FALSE(token2.empty());
    EXPECT_NE(token1, token2);

    auto retrievedUser1 = SessionManager::instance().getUser(token1);
    auto retrievedUser2 = SessionManager::instance().getUser(token2);

    ASSERT_TRUE(retrievedUser1.has_value());
    ASSERT_TRUE(retrievedUser2.has_value());
    EXPECT_EQ(retrievedUser1->username, "user1");
    EXPECT_EQ(retrievedUser2->username, "user2");
}

TEST_F(SessionManagerTest, ValidateSession_ValidToken) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    EXPECT_TRUE(SessionManager::instance().validateSession(token));
}

TEST_F(SessionManagerTest, ValidateSession_EmptyToken) {
    EXPECT_FALSE(SessionManager::instance().validateSession(""));
}

TEST_F(SessionManagerTest, ValidateSession_InvalidToken) {
    EXPECT_FALSE(SessionManager::instance().validateSession("nonexistent_token_12345"));
}

TEST_F(SessionManagerTest, ValidateSession_DestroyedSession) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    EXPECT_TRUE(SessionManager::instance().validateSession(token));
    SessionManager::instance().destroySession(token);
    EXPECT_FALSE(SessionManager::instance().validateSession(token));
}

TEST_F(SessionManagerTest, GetUser_ValidToken) {
    User user = createTestUser("testuser", UserRole::Admin);
    std::string token = SessionManager::instance().createSession(user);

    auto retrievedUser = SessionManager::instance().getUser(token);

    ASSERT_TRUE(retrievedUser.has_value());
    EXPECT_EQ(retrievedUser->username, "testuser");
    EXPECT_EQ(retrievedUser->role, UserRole::Admin);
    EXPECT_EQ(retrievedUser->password_hash, "test_hash");
}

TEST_F(SessionManagerTest, GetUser_InvalidToken) {
    auto user = SessionManager::instance().getUser("invalid_token");
    EXPECT_FALSE(user.has_value());
}

TEST_F(SessionManagerTest, GetUser_EmptyToken) {
    auto user = SessionManager::instance().getUser("");
    EXPECT_FALSE(user.has_value());
}

TEST_F(SessionManagerTest, DestroySession_ExistingSession) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    EXPECT_TRUE(SessionManager::instance().destroySession(token));
    EXPECT_FALSE(SessionManager::instance().validateSession(token));
}

TEST_F(SessionManagerTest, DestroySession_NonExistingSession) {
    EXPECT_FALSE(SessionManager::instance().destroySession("nonexistent_token"));
}

TEST_F(SessionManagerTest, DestroySession_AlreadyDestroyed) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    EXPECT_TRUE(SessionManager::instance().destroySession(token));
    EXPECT_FALSE(SessionManager::instance().destroySession(token));
}

TEST_F(SessionManagerTest, SessionCount_Initial) {
    SessionManager::instance().cleanExpiredSessions();
    EXPECT_EQ(SessionManager::instance().sessionCount(), 0);
}

TEST_F(SessionManagerTest, SessionCount_AfterCreate) {
    User user1 = createTestUser("user1");
    User user2 = createTestUser("user2");

    SessionManager::instance().createSession(user1);
    SessionManager::instance().createSession(user2);

    EXPECT_EQ(SessionManager::instance().sessionCount(), 2);
}

TEST_F(SessionManagerTest, SessionCount_AfterDestroy) {
    User user1 = createTestUser("user1");
    User user2 = createTestUser("user2");

    std::string token1 = SessionManager::instance().createSession(user1);
    SessionManager::instance().createSession(user2);

    SessionManager::instance().destroySession(token1);
    EXPECT_EQ(SessionManager::instance().sessionCount(), 1);
}

TEST_F(SessionManagerTest, CleanExpiredSessions_NoExpiredSessions) {
    User user = createTestUser("testuser");
    SessionManager::instance().createSession(user);

    SessionManager::instance().cleanExpiredSessions();
    EXPECT_EQ(SessionManager::instance().sessionCount(), 1);
}

TEST_F(SessionManagerTest, ConcurrentCreateSession) {
    const int numThreads = 4;
    const int sessionsPerThread = 10;
    std::vector<std::string> tokens;
    std::mutex tokensMutex;

    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &tokens, &tokensMutex, t, sessionsPerThread]() {
            User user = createTestUser("user_thread_" + std::to_string(t));
            for (int i = 0; i < sessionsPerThread; ++i) {
                std::string token = SessionManager::instance().createSession(user);
                std::lock_guard<std::mutex> lock(tokensMutex);
                tokens.push_back(token);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(static_cast<int>(tokens.size()), numThreads * sessionsPerThread);
    for (const auto& token : tokens) {
        EXPECT_TRUE(SessionManager::instance().validateSession(token));
    }
}

TEST_F(SessionManagerTest, ConcurrentValidateSession) {
    User user = createTestUser("testuser");
    std::vector<std::string> tokens;
    for (int i = 0; i < 20; ++i) {
        tokens.push_back(SessionManager::instance().createSession(user));
    }

    std::atomic<int> validCount{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&tokens, &validCount]() {
            for (int i = 0; i < 100; ++i) {
                for (const auto& token : tokens) {
                    if (SessionManager::instance().validateSession(token)) {
                        validCount++;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(validCount, 4 * 100 * static_cast<int>(tokens.size()));
}

TEST_F(SessionManagerTest, ConcurrentDestroySession) {
    std::vector<std::string> tokens;
    for (int i = 0; i < 20; ++i) {
        User user = createTestUser("user_" + std::to_string(i));
        tokens.push_back(SessionManager::instance().createSession(user));
    }

    std::atomic<int> destroyCount{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&tokens, &destroyCount, t]() {
            for (int i = t; i < static_cast<int>(tokens.size()); i += 4) {
                if (SessionManager::instance().destroySession(tokens[i])) {
                    destroyCount++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(destroyCount, static_cast<int>(tokens.size()));
}

TEST_F(SessionManagerTest, ConcurrentMixedOperations) {
    std::mutex tokensMutex;
    std::vector<std::string> tokens;
    std::atomic<int> createCount{0};
    std::atomic<int> validateCount{0};
    std::atomic<int> destroyCount{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([this, &tokens, &tokensMutex, &createCount, &validateCount, &destroyCount, t]() {
            for (int i = 0; i < 50; ++i) {
                User user = createTestUser("user_" + std::to_string(t) + "_" + std::to_string(i));
                std::string token = SessionManager::instance().createSession(user);
                {
                    std::lock_guard<std::mutex> lock(tokensMutex);
                    tokens.push_back(token);
                }
                createCount++;

                if (SessionManager::instance().validateSession(token)) {
                    validateCount++;
                }

                if (i % 2 == 0) {
                    if (SessionManager::instance().destroySession(token)) {
                        destroyCount++;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(createCount, 200);
    EXPECT_EQ(destroyCount, 100);
}
