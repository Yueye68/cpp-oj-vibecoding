#include "auth_middleware.h"
#include "logger.h"
#include "session.h"
#include "../models/user.h"

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
    return SessionManager::instance().getUser(token);
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
