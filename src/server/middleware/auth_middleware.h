#pragma once

#include "httplib.h"
#include <functional>
#include <optional>
#include "../models/user.h"

using CheckAuthFn = std::function<std::optional<User>(const httplib::Request&)>;

class AuthMiddleware {
public:
    static std::optional<User> getCurrentUser(const httplib::Request& req);
    static bool isAdmin(const httplib::Request& req);
    static bool requireAuth(const httplib::Request& req, httplib::Response& res);
    static bool requireAdmin(const httplib::Request& req, httplib::Response& res);
};

std::string getSessionToken(const httplib::Request& req);
