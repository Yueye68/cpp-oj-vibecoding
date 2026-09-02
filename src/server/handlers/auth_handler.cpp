#include "auth_handler.h"
#include "auth_middleware.h"
#include "logger.h"
#include "password.h"
#include "session.h"
#include "user.h"
#include "../models/user.h"
#include "json.h"
#include <sstream>

static bool parseJson(const std::string& body, Json::Value& json) {
    std::istringstream iss(body);
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    Json::String err;
    return Json::parseFromStream(builder, iss, &json, &err);
}

void handleRegister(const httplib::Request& req, httplib::Response& res) {
    Json::Value json;
    if (!parseJson(req.body, json)) {
        res.status = 400;
        res.set_content("{\"error\": \"Invalid JSON\"}", "application/json");
        return;
    }

    std::string username = json["username"].asString();
    std::string password = json["password"].asString();

    if (username.empty() || password.empty()) {
        res.status = 400;
        res.set_content("{\"error\": \"Username and password are required\"}", "application/json");
        return;
    }

    if (username.length() > 50) {
        res.status = 400;
        std::string errorMsg = "{\"error\": \"Username too long (max 50 characters)\"}";
        res.set_content(errorMsg, "application/json");
        return;
    }

    auto existing = User::findByUsername(username);
    if (existing.has_value()) {
        res.status = 409;
        res.set_content("{\"error\": \"Username already exists\"}", "application/json");
        return;
    }

    User user;
    user.username = username;
    user.password_hash = PasswordUtil::hash(password);
    user.role = UserRole::User;

    if (!user.create()) {
        Logger::instance().error("Failed to create user: " + username);
        res.status = 500;
        res.set_content("{\"error\": \"Failed to create user\"}", "application/json");
        return;
    }

    Logger::instance().info("User registered: " + username);
    res.status = 201;
    res.set_content("{\"message\": \"User registered successfully\"}", "application/json");
}

void handleLogin(const httplib::Request& req, httplib::Response& res) {
    Json::Value json;
    if (!parseJson(req.body, json)) {
        res.status = 400;
        res.set_content("{\"error\": \"Invalid JSON\"}", "application/json");
        return;
    }

    std::string username = json["username"].asString();
    std::string password = json["password"].asString();

    if (username.empty() || password.empty()) {
        res.status = 400;
        res.set_content("{\"error\": \"Username and password are required\"}", "application/json");
        return;
    }

    auto userOpt = User::findByUsername(username);
    if (!userOpt.has_value()) {
        res.status = 401;
        res.set_content("{\"error\": \"Invalid credentials\"}", "application/json");
        return;
    }

    User& user = userOpt.value();
    if (!PasswordUtil::verify(password, user.password_hash)) {
        res.status = 401;
        res.set_content("{\"error\": \"Invalid credentials\"}", "application/json");
        return;
    }

    std::string token = SessionManager::instance().createSession(user);
    Logger::instance().info("User logged in: " + username);

    Json::Value response;
    response["message"] = "Login successful";
    response["user"]["id"] = user.id;
    response["user"]["username"] = user.username;
    response["user"]["role"] = User::roleToString(user.role);

    res.set_header("Set-Cookie", "session_token=" + token + "; HttpOnly; Path=/");
    res.set_content(response.toStyledString(), "application/json");
}

void handleLogout(const httplib::Request& req, httplib::Response& res) {
    std::string token = getSessionToken(req);
    if (!token.empty()) {
        SessionManager::instance().destroySession(token);
    }

    Logger::instance().info("User logged out");
    res.status = 200;
    res.set_header("Set-Cookie", "session_token=; HttpOnly; Path=/; Max-Age=0");
    res.set_content("{\"message\": \"Logout successful\"}", "application/json");
}

void handleGetCurrentUser(const httplib::Request& req, httplib::Response& res) {
    auto userOpt = AuthMiddleware::getCurrentUser(req);
    if (!userOpt.has_value()) {
        res.status = 401;
        res.set_content("{\"error\": \"Not authenticated\"}", "application/json");
        return;
    }

    User& user = userOpt.value();
    Json::Value response;
    response["id"] = user.id;
    response["username"] = user.username;
    response["role"] = User::roleToString(user.role);
    response["created_at"] = user.created_at;

    res.set_content(response.toStyledString(), "application/json");
}
