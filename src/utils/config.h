#pragma once

#include <string>
#include <map>
#include <memory>

struct DatabaseConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string username = "root";
    std::string password = "";
    std::string database = "oj_system";
    std::string charset = "utf8mb4";
    std::string collation = "utf8mb4_unicode_ci";
};

struct ServerConfig {
    std::string host = "0.0.0.0";
    int port = 8080;
};

struct AppConfig {
    std::string test_case_dir = "./test_cases";
    std::string upload_dir = "./uploads";
    std::string log_level = "INFO";
    std::string log_file = "./logs/app.log";
};

class Config {
public:
    static Config& instance();

    void reset();
    bool load(const std::string& filepath);
    bool loadFromString(const std::string& content);

    const DatabaseConfig& database() const { return db_config_; }
    const ServerConfig& server() const { return server_config_; }
    const AppConfig& app() const { return app_config_; }

    DatabaseConfig& database() { return db_config_; }
    ServerConfig& server() { return server_config_; }
    AppConfig& app() { return app_config_; }

private:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    bool parseYAML(const std::string& content);
    std::string trim(const std::string& s);
    std::string trimQuotes(const std::string& s);

    DatabaseConfig db_config_;
    ServerConfig server_config_;
    AppConfig app_config_;
};