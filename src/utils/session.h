#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <random>
#include "models/user.h"

class SessionManager {
public:
    static SessionManager& instance();

    struct SessionData {
        User user;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::seconds timeout;

        SessionData() : timeout(std::chrono::seconds(86400)) {}
        SessionData(const User& u, std::chrono::seconds t = std::chrono::seconds(86400))
            : user(u), created_at(std::chrono::steady_clock::now()), timeout(t) {}

        bool isExpired() const {
            return std::chrono::steady_clock::now() - created_at > timeout;
        }
    };

    std::string createSession(const User& user);
    bool validateSession(const std::string& token);
    std::optional<User> getUser(const std::string& token);
    bool destroySession(const std::string& token);
    void cleanExpiredSessions();
    void clearAllSessions();

    size_t sessionCount() const;

private:
    SessionManager() = default;
    ~SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    std::string generateToken();
    std::string generateTokenInternal();

    std::unordered_map<std::string, SessionData> sessions_;
    mutable std::mutex mutex_;
    std::random_device rd_;
    std::mt19937_64 gen_;
    std::uniform_int_distribution<long long> dist_;
};
