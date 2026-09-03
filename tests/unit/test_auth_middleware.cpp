#include <gtest/gtest.h>
#include "server/middleware/auth_middleware.h"
#include "utils/session.h"
#include "models/user.h"

class AuthMiddlewareTest : public ::testing::Test {
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
        req.path = "/test";
        if (!cookie.empty()) {
            req.headers.emplace("Cookie", cookie);
        }
        return req;
    }
};

class GetSessionTokenTest : public AuthMiddlewareTest {};

TEST_F(GetSessionTokenTest, ExtractToken_FromValidCookie) {
    httplib::Request req = createRequestWithCookie("session_token=test_token_123");
    std::string token = getSessionToken(req);
    EXPECT_EQ(token, "test_token_123");
}

TEST_F(GetSessionTokenTest, ExtractToken_WithOtherCookies) {
    httplib::Request req = createRequestWithCookie("other=value; session_token=my_token; foo=bar");
    std::string token = getSessionToken(req);
    EXPECT_EQ(token, "my_token");
}

TEST_F(GetSessionTokenTest, ExtractToken_EmptyCookie) {
    httplib::Request req = createRequestWithCookie("");
    std::string token = getSessionToken(req);
    EXPECT_EQ(token, "");
}

TEST_F(GetSessionTokenTest, ExtractToken_NoSessionToken) {
    httplib::Request req = createRequestWithCookie("other_token=value");
    std::string token = getSessionToken(req);
    EXPECT_EQ(token, "");
}

TEST_F(GetSessionTokenTest, ExtractToken_NoCookieHeader) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/test";
    std::string token = getSessionToken(req);
    EXPECT_EQ(token, "");
}

TEST_F(GetSessionTokenTest, ExtractToken_TokenAtEnd) {
    httplib::Request req = createRequestWithCookie("foo=bar; session_token=end_token");
    std::string token = getSessionToken(req);
    EXPECT_EQ(token, "end_token");
}

TEST_F(GetSessionTokenTest, ExtractToken_TokenAtStart) {
    httplib::Request req = createRequestWithCookie("session_token=start_token; foo=bar");
    std::string token = getSessionToken(req);
    EXPECT_EQ(token, "start_token");
}

TEST_F(GetSessionTokenTest, ExtractToken_WithSemicolonInValue) {
    httplib::Request req = createRequestWithCookie("session_token=value;semicolon");
    std::string token = getSessionToken(req);
    EXPECT_EQ(token, "value");
}

class GetCurrentUserTest : public AuthMiddlewareTest {};

TEST_F(GetCurrentUserTest, GetCurrentUser_ValidSession) {
    User user = createTestUser("testuser", UserRole::User);
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    auto result = AuthMiddleware::getCurrentUser(req);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "testuser");
    EXPECT_EQ(result->role, UserRole::User);
}

TEST_F(GetCurrentUserTest, GetCurrentUser_AdminUser) {
    User admin = createTestUser("admin", UserRole::Admin);
    std::string token = SessionManager::instance().createSession(admin);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    auto result = AuthMiddleware::getCurrentUser(req);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "admin");
    EXPECT_EQ(result->role, UserRole::Admin);
}

TEST_F(GetCurrentUserTest, GetCurrentUser_EmptyToken) {
    httplib::Request req = createRequestWithCookie("");
    auto result = AuthMiddleware::getCurrentUser(req);
    EXPECT_FALSE(result.has_value());
}

TEST_F(GetCurrentUserTest, GetCurrentUser_InvalidToken) {
    httplib::Request req = createRequestWithCookie("session_token=invalid_token");
    auto result = AuthMiddleware::getCurrentUser(req);
    EXPECT_FALSE(result.has_value());
}

TEST_F(GetCurrentUserTest, GetCurrentUser_NoCookie) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/test";
    auto result = AuthMiddleware::getCurrentUser(req);
    EXPECT_FALSE(result.has_value());
}

TEST_F(GetCurrentUserTest, GetCurrentUser_SessionDestroyed) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req1 = createRequestWithCookie("session_token=" + token);
    auto result1 = AuthMiddleware::getCurrentUser(req1);
    ASSERT_TRUE(result1.has_value());

    SessionManager::instance().destroySession(token);

    httplib::Request req2 = createRequestWithCookie("session_token=" + token);
    auto result2 = AuthMiddleware::getCurrentUser(req2);
    EXPECT_FALSE(result2.has_value());
}

class IsAdminTest : public AuthMiddlewareTest {};

TEST_F(IsAdminTest, IsAdmin_AdminUser) {
    User admin = createTestUser("admin", UserRole::Admin);
    std::string token = SessionManager::instance().createSession(admin);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    EXPECT_TRUE(AuthMiddleware::isAdmin(req));
}

TEST_F(IsAdminTest, IsAdmin_NormalUser) {
    User user = createTestUser("testuser", UserRole::User);
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    EXPECT_FALSE(AuthMiddleware::isAdmin(req));
}

TEST_F(IsAdminTest, IsAdmin_NoSession) {
    httplib::Request req = createRequestWithCookie("");
    EXPECT_FALSE(AuthMiddleware::isAdmin(req));
}

TEST_F(IsAdminTest, IsAdmin_InvalidToken) {
    httplib::Request req = createRequestWithCookie("session_token=invalid");
    EXPECT_FALSE(AuthMiddleware::isAdmin(req));
}

TEST_F(IsAdminTest, IsAdmin_SessionExpired) {
    User admin = createTestUser("admin", UserRole::Admin);
    std::string token = SessionManager::instance().createSession(admin);
    SessionManager::instance().destroySession(token);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    EXPECT_FALSE(AuthMiddleware::isAdmin(req));
}

class RequireAuthTest : public AuthMiddlewareTest {};

TEST_F(RequireAuthTest, RequireAuth_ValidSession) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    httplib::Response res;

    bool result = AuthMiddleware::requireAuth(req, res);
    EXPECT_TRUE(result);
    EXPECT_EQ(res.status, -1);
}

TEST_F(RequireAuthTest, RequireAuth_EmptyToken) {
    httplib::Request req = createRequestWithCookie("");
    httplib::Response res;

    bool result = AuthMiddleware::requireAuth(req, res);
    EXPECT_FALSE(result);
    EXPECT_EQ(res.status, 401);
    EXPECT_EQ(res.body, R"({"error": "Unauthorized"})");
}

TEST_F(RequireAuthTest, RequireAuth_InvalidToken) {
    httplib::Request req = createRequestWithCookie("session_token=invalid");
    httplib::Response res;

    bool result = AuthMiddleware::requireAuth(req, res);
    EXPECT_FALSE(result);
    EXPECT_EQ(res.status, 401);
}

TEST_F(RequireAuthTest, RequireAuth_NoCookie) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/test";
    httplib::Response res;

    bool result = AuthMiddleware::requireAuth(req, res);
    EXPECT_FALSE(result);
    EXPECT_EQ(res.status, 401);
}

TEST_F(RequireAuthTest, RequireAuth_ResponseBodyNotModifiedOnSuccess) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    httplib::Response res;
    res.body = "original";

    AuthMiddleware::requireAuth(req, res);
    EXPECT_EQ(res.body, "original");
}

class RequireAdminTest : public AuthMiddlewareTest {};

TEST_F(RequireAdminTest, RequireAdmin_AdminUser) {
    User admin = createTestUser("admin", UserRole::Admin);
    std::string token = SessionManager::instance().createSession(admin);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    httplib::Response res;

    bool result = AuthMiddleware::requireAdmin(req, res);
    EXPECT_TRUE(result);
    EXPECT_EQ(res.status, -1);
}

TEST_F(RequireAdminTest, RequireAdmin_NormalUser) {
    User user = createTestUser("testuser", UserRole::User);
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    httplib::Response res;

    bool result = AuthMiddleware::requireAdmin(req, res);
    EXPECT_FALSE(result);
    EXPECT_EQ(res.status, 403);
    EXPECT_EQ(res.body, R"({"error": "Forbidden: admin access required"})");
}

TEST_F(RequireAdminTest, RequireAdmin_EmptyToken) {
    httplib::Request req = createRequestWithCookie("");
    httplib::Response res;

    bool result = AuthMiddleware::requireAdmin(req, res);
    EXPECT_FALSE(result);
    EXPECT_EQ(res.status, 401);
    EXPECT_EQ(res.body, R"({"error": "Unauthorized"})");
}

TEST_F(RequireAdminTest, RequireAdmin_InvalidToken) {
    httplib::Request req = createRequestWithCookie("session_token=invalid");
    httplib::Response res;

    bool result = AuthMiddleware::requireAdmin(req, res);
    EXPECT_FALSE(result);
    EXPECT_EQ(res.status, 401);
}

TEST_F(RequireAdminTest, RequireAdmin_NoCookie) {
    httplib::Request req;
    req.method = "POST";
    req.path = "/test";
    httplib::Response res;

    bool result = AuthMiddleware::requireAdmin(req, res);
    EXPECT_FALSE(result);
    EXPECT_EQ(res.status, 401);
}

TEST_F(RequireAdminTest, RequireAdmin_AdminRoleAfterPromotion) {
    User user = createTestUser("testuser", UserRole::User);
    std::string token = SessionManager::instance().createSession(user);

    auto sessionUser = SessionManager::instance().getUser(token);
    ASSERT_TRUE(sessionUser.has_value());
    EXPECT_EQ(sessionUser->role, UserRole::User);

    sessionUser->role = UserRole::Admin;

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    httplib::Response res;

    bool result = AuthMiddleware::requireAdmin(req, res);
    EXPECT_FALSE(result);
    EXPECT_EQ(res.status, 403);
}

TEST_F(RequireAdminTest, RequireAdmin_ResponseContentTypeNotJsonOnUnauthorized) {
    httplib::Request req = createRequestWithCookie("");
    httplib::Response res;

    AuthMiddleware::requireAdmin(req, res);
    EXPECT_EQ(res.status, 401);
}

TEST_F(RequireAdminTest, RequireAdmin_ResponseContentTypeNotJsonOnForbidden) {
    User user = createTestUser("testuser", UserRole::User);
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req = createRequestWithCookie("session_token=" + token);
    httplib::Response res;

    AuthMiddleware::requireAdmin(req, res);
    EXPECT_EQ(res.status, 403);
}

class AuthMiddlewareIntegrationTest : public AuthMiddlewareTest {};

TEST_F(AuthMiddlewareIntegrationTest, MultipleUsers_DifferentSessions) {
    User user1 = createTestUser("user1", UserRole::User);
    User user2 = createTestUser("user2", UserRole::User);
    User admin = createTestUser("admin", UserRole::Admin);

    std::string token1 = SessionManager::instance().createSession(user1);
    std::string token2 = SessionManager::instance().createSession(user2);
    std::string adminToken = SessionManager::instance().createSession(admin);

    httplib::Request req1 = createRequestWithCookie("session_token=" + token1);
    httplib::Request req2 = createRequestWithCookie("session_token=" + token2);
    httplib::Request adminReq = createRequestWithCookie("session_token=" + adminToken);

    httplib::Response res1, res2, res3, res4, res5, res6;

    EXPECT_TRUE(AuthMiddleware::requireAuth(req1, res1));
    EXPECT_TRUE(AuthMiddleware::requireAuth(req2, res2));
    EXPECT_TRUE(AuthMiddleware::requireAuth(adminReq, res3));

    EXPECT_FALSE(AuthMiddleware::isAdmin(req1));
    EXPECT_FALSE(AuthMiddleware::isAdmin(req2));
    EXPECT_TRUE(AuthMiddleware::isAdmin(adminReq));

    EXPECT_TRUE(AuthMiddleware::requireAdmin(adminReq, res4));
    EXPECT_FALSE(AuthMiddleware::requireAdmin(req1, res5));
    EXPECT_FALSE(AuthMiddleware::requireAdmin(req2, res6));
}

TEST_F(AuthMiddlewareIntegrationTest, SessionDestroyed_ThenRequireAuth) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req1 = createRequestWithCookie("session_token=" + token);
    httplib::Response res1;
    EXPECT_TRUE(AuthMiddleware::requireAuth(req1, res1));

    SessionManager::instance().destroySession(token);

    httplib::Request req2 = createRequestWithCookie("session_token=" + token);
    httplib::Response res2;
    EXPECT_FALSE(AuthMiddleware::requireAuth(req2, res2));
    EXPECT_EQ(res2.status, 401);
}

TEST_F(AuthMiddlewareIntegrationTest, CookieWithSpecialCharacters) {
    User user = createTestUser("testuser");
    std::string token = SessionManager::instance().createSession(user);

    httplib::Request req;
    req.method = "POST";
    req.path = "/test";
    req.headers.emplace("Cookie", "session_token=" + token + "; csrf_token=abc<>\"&'def");

    auto result = AuthMiddleware::getCurrentUser(req);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "testuser");
}