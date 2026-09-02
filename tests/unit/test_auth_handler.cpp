#include <gtest/gtest.h>
#include "server/handlers/auth_handler.h"
#include "utils/session.h"
#include "models/user.h"
#include <iostream>

class AuthHandlerLogoutTest : public ::testing::Test {
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

    httplib::Request createRequestWithCookie(const std::string& cookie) {
        httplib::Request req;
        req.method = "POST";
        req.path = "/api/auth/logout";
        if (!cookie.empty()) {
            req.headers.emplace("Cookie", cookie);
        }
        return req;
    }
};

TEST_F(AuthHandlerLogoutTest, Logout_WithValidSession) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    ASSERT_TRUE(SessionManager::instance().validateSession(token));

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    httplib::Response res;

    handleLogout(req, res);

    EXPECT_EQ(200, res.status);
    EXPECT_EQ("{\"message\": \"Logout successful\"}", res.body);

    EXPECT_FALSE(SessionManager::instance().validateSession(token));

    bool hasCookieHeader = false;
    for (const auto& header : res.headers) {
        if (header.first == "Set-Cookie") {
            hasCookieHeader = true;
            std::string cookie = header.second;
            EXPECT_TRUE(cookie.find("session_token=;") != std::string::npos);
            EXPECT_TRUE(cookie.find("Max-Age=0") != std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(hasCookieHeader);
}

TEST_F(AuthHandlerLogoutTest, Logout_WithEmptySessionToken) {
    httplib::Request req = createRequestWithCookie("");
    httplib::Response res;

    handleLogout(req, res);

    EXPECT_EQ(200, res.status);
    EXPECT_EQ("{\"message\": \"Logout successful\"}", res.body);
}

TEST_F(AuthHandlerLogoutTest, Logout_WithInvalidSessionToken) {
    httplib::Request req = createRequestWithCookie("session_token=invalid_token_12345");
    httplib::Response res;

    handleLogout(req, res);

    EXPECT_EQ(200, res.status);
    EXPECT_EQ("{\"message\": \"Logout successful\"}", res.body);
}

TEST_F(AuthHandlerLogoutTest, Logout_WithNoCookieHeader) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/api/auth/logout";
    httplib::Response res;

    handleLogout(req, res);

    EXPECT_EQ(200, res.status);
    EXPECT_EQ("{\"message\": \"Logout successful\"}", res.body);
}

TEST_F(AuthHandlerLogoutTest, Logout_AlreadyLoggedOutSession) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req1 = createRequestWithCookie("session_token=" + token);
    httplib::Response res1;
    handleLogout(req1, res1);

    EXPECT_EQ(200, res1.status);
    EXPECT_FALSE(SessionManager::instance().validateSession(token));

    httplib::Request req2 = createRequestWithCookie("session_token=" + token);
    httplib::Response res2;
    handleLogout(req2, res2);

    EXPECT_EQ(200, res2.status);
    EXPECT_EQ("{\"message\": \"Logout successful\"}", res2.body);
}

TEST_F(AuthHandlerLogoutTest, Logout_WithAdminUser) {
    User admin = createTestUser("admin", UserRole::Admin);
    std::string token = SessionManager::instance().createSession(admin);

    ASSERT_TRUE(SessionManager::instance().validateSession(token));

    auto sessionUser = SessionManager::instance().getUser(token);
    ASSERT_TRUE(sessionUser.has_value());
    EXPECT_EQ(sessionUser->role, UserRole::Admin);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    httplib::Response res;

    handleLogout(req, res);

    EXPECT_EQ(200, res.status);
    EXPECT_FALSE(SessionManager::instance().validateSession(token));
}

TEST_F(AuthHandlerLogoutTest, Logout_MultipleSessionsSameUser) {
    User user = createTestUser("testuser");
    std::string token1 = SessionManager::instance().createSession(user);
    std::string token2 = SessionManager::instance().createSession(user);

    ASSERT_TRUE(SessionManager::instance().validateSession(token1));
    ASSERT_TRUE(SessionManager::instance().validateSession(token2));

    httplib::Request req = createRequestWithCookie("session_token=" + token1);
    httplib::Response res;

    handleLogout(req, res);

    EXPECT_EQ(200, res.status);
    EXPECT_FALSE(SessionManager::instance().validateSession(token1));
    EXPECT_TRUE(SessionManager::instance().validateSession(token2));
}

TEST_F(AuthHandlerLogoutTest, Logout_CookieWithOtherValues) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/auth/logout";
    req.headers.emplace("Cookie", "other_cookie=value; session_token=" + token + "; another=test");
    httplib::Response res;

    handleLogout(req, res);

    EXPECT_EQ(200, res.status);
    EXPECT_FALSE(SessionManager::instance().validateSession(token));
}

TEST_F(AuthHandlerLogoutTest, Logout_CookieWithoutHttpOnly) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req;
    req.method = "POST";
    req.path = "/api/auth/logout";
    req.headers.emplace("Cookie", "session_token=" + token);
    httplib::Response res;

    handleLogout(req, res);

    EXPECT_EQ(200, res.status);
    EXPECT_FALSE(SessionManager::instance().validateSession(token));
}
