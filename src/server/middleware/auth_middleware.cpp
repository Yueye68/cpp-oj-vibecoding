#include "auth_middleware.h"
#include "logger.h"
#include "../models/user.h"
#include <unordered_map>
#include <mutex>
#include <random>
#include <chrono>

static std::unordered_map<std::string, User> g_sessions;
static std::mutex g_sessions_mutex;
static std::random_device g_rd;
static std::mt19937_64 g_gen(g_rd());

std::string generateSessionToken() {
    std::uniform_int_distribution<long long> dist(0, 9223372036854775807LL);
    return std::to_string(dist(g_gen));
}

std::string getSessionToken(const httplib::Request& req) {
    auto cookie_it = req.headers.find("Cookie");
    if (cookie_it == req.headers.end()) {
        return "";
    }

    std::string cookies = cookie_it->second;
    std::string prefix = "session_token=";
    size_t pos = cookies.find(prefix);
    if (pos == std::string::npos) {
        return "";
    }
    pos += prefix.size();
    size_t end = cookies.find(";", pos);
    if (end == std::string::npos) {
        return cookies.substr(pos);
    }
    return cookies.substr(pos, end - pos);
}

std::optional<User> AuthMiddleware::getCurrentUser(const httplib::Request& req) {
    std::string token = getSessionToken(req);
    if (token.empty()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(g_sessions_mutex);
    auto it = g_sessions.find(token);
    if (it == g_sessions.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool AuthMiddleware::isAdmin(const httplib::Request& req) {
    auto user = getCurrentUser(req);
    if (!user) {
        return false;
    }
    return user->role == UserRole::Admin;
}

bool AuthMiddleware::requireAuth(const httplib::Request& req, httplib::Response& res) {
    auto user = getCurrentUser(req);
    if (!user) {
        res.status = 401;
        res.set_content(R"({"error": "Unauthorized"})", "application/json");
        return false;
    }
    return true;
}

bool AuthMiddleware::requireAdmin(const httplib::Request& req, httplib::Response& res) {
    auto user = getCurrentUser(req);
    if (!user) {
        res.status = 401;
        res.set_content(R"({"error": "Unauthorized"})", "application/json");
        return false;
    }
    if (user->role != UserRole::Admin) {
        res.status = 403;
        res.set_content(R"({"error": "Forbidden: admin access required"})", "application/json");
        return false;
    }
    return true;
}

std::string createSession(const User& user) {
    std::string token = generateSessionToken();
    std::lock_guard<std::mutex> lock(g_sessions_mutex);
    g_sessions[token] = user;
    return token;
}

void destroySession(const std::string& token) {
    std::lock_guard<std::mutex> lock(g_sessions_mutex);
    g_sessions.erase(token);
}
