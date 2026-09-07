#include "session.h"
#include "logger.h"

SessionManager& SessionManager::instance() {
    static SessionManager instance;
    return instance;
}

std::string SessionManager::generateToken() {
    return generateTokenInternal();
}

std::string SessionManager::generateTokenInternal() {
    return std::to_string(dist_(gen_));
}

std::string SessionManager::createSession(const User& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string token = generateToken();
    sessions_[token] = SessionData(user);
    Logger::instance().info("Session created for user: " + user.username + ", token: " + token);
    return token;
}

bool SessionManager::validateSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return false;
    }
    if (it->second.isExpired()) {
        sessions_.erase(it);
        return false;
    }
    return true;
}

std::optional<User> SessionManager::getUser(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return std::nullopt;
    }
    if (it->second.isExpired()) {
        sessions_.erase(it);
        return std::nullopt;
    }
    return it->second.user;
}

bool SessionManager::destroySession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) {
        return false;
    }
    sessions_.erase(it);
    Logger::instance().info("Session destroyed, token: " + token);
    return true;
}

int SessionManager::destroySessionsByUserId(int userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    int n = 0;
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (it->second.user.id == userId) {
            it = sessions_.erase(it);
            ++n;
        } else {
            ++it;
        }
    }
    if (n > 0) {
        Logger::instance().info("Destroyed " + std::to_string(n) + " session(s) for user id: " + std::to_string(userId));
    }
    return n;
}

void SessionManager::cleanExpiredSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (it->second.isExpired()) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void SessionManager::clearAllSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
}

size_t SessionManager::sessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}
