#pragma once

#include <string>
#include <optional>
#include <mysql/mysql.h>
#include "connection_pool.h"

enum class UserRole {
    User,
    Admin
};

struct User {
    int id = 0;
    std::string username;
    std::string password_hash;
    UserRole role = UserRole::User;
    std::string created_at;

    static std::string roleToString(UserRole r);
    static UserRole stringToRole(const std::string& s);

    bool create();
    bool update();
    bool remove();
    bool loadFromDB(int id);
    bool saveToDB();

    static std::optional<User> findById(int id);
    static std::optional<User> findByUsername(const std::string& username);
    static std::vector<User> findAll(int page = 1, int pageSize = 20);
    static int count();
};
