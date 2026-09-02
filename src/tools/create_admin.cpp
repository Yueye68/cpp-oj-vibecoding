#include <iostream>
#include "config.h"
#include "password.h"
#include "user.h"
#include "connection_pool.h"
#include "logger.h"

int main(int argc, char* argv[]) {
    std::string configPath = "config/config.yaml";
    if (argc > 1) {
        configPath = argv[1];
    }

    std::cout << "Loading config from " << configPath << "..." << std::endl;
    if (!Config::instance().load(configPath)) {
        std::cerr << "Failed to load " << configPath << std::endl;
        return 1;
    }

    std::cout << "Initializing database connection pool..." << std::endl;
    ConnectionPool::instance().init(Config::instance().database());

    auto existingAdmin = User::findByUsername("admin");
    if (existingAdmin.has_value()) {
        std::cout << "Admin user exists, deleting..." << std::endl;
        User admin = existingAdmin.value();
        if (!admin.remove()) {
            std::cerr << "Failed to delete existing admin user" << std::endl;
            return 1;
        }
        std::cout << "Existing admin user deleted." << std::endl;
    }

    std::cout << "Hashing password..." << std::endl;
    std::string hashedPassword = PasswordUtil::hash("admin123");
    if (hashedPassword.empty()) {
        std::cerr << "Failed to hash password" << std::endl;
        return 1;
    }

    User newAdmin;
    newAdmin.username = "admin";
    newAdmin.password_hash = hashedPassword;
    newAdmin.role = UserRole::Admin;

    std::cout << "Creating admin user..." << std::endl;
    if (!newAdmin.create()) {
        std::cerr << "Failed to create admin user" << std::endl;
        return 1;
    }

    std::cout << "Admin user created successfully!" << std::endl;
    std::cout << "Username: admin" << std::endl;
    std::cout << "Password: admin123" << std::endl;
    std::cout << "Password hash: " << hashedPassword << std::endl;

    return 0;
}