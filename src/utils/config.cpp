#include "config.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

Config& Config::instance() {
    static Config instance;
    return instance;
}

void Config::reset() {
    db_config_ = DatabaseConfig();
    server_config_ = ServerConfig();
    app_config_ = AppConfig();
}

bool Config::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseYAML(buffer.str());
}

bool Config::loadFromString(const std::string& content) {
    return parseYAML(content);
}

std::string Config::trim(const std::string& s) {
    auto start = std::find_if(s.begin(), s.end(), [](unsigned char c) {
        return !std::isspace(c);
    });
    auto end = std::find_if(s.rbegin(), s.rend(), [](unsigned char c) {
        return !std::isspace(c);
    }).base();

    if (start >= end) return "";

    std::string result(start, end);
    result.erase(0, result.find_first_not_of(" \t\r\n"));
    result.erase(result.find_last_not_of(" \t\r\n") + 1);
    return result;
}

std::string Config::trimQuotes(const std::string& s) {
    if (s.size() >= 2) {
        if ((s.front() == '"' && s.back() == '"') ||
            (s.front() == '\'' && s.back() == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

bool Config::parseYAML(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    std::map<std::string, std::map<std::string, std::string>> sections;

    while (std::getline(stream, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line.back() == ':') {
            current_section = trim(line.substr(0, line.size() - 1));
            continue;
        }

        if (!current_section.empty()) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = trim(line.substr(0, colon_pos));
                std::string value = trim(line.substr(colon_pos + 1));
                value = trimQuotes(value);
                sections[current_section][key] = value;
            }
        }
    }

    if (sections.find("database") != sections.end()) {
        const auto& db = sections["database"];
        if (db.count("host")) db_config_.host = db.at("host");
        if (db.count("port")) db_config_.port = std::stoi(db.at("port"));
        if (db.count("username")) db_config_.username = db.at("username");
        if (db.count("password")) db_config_.password = db.at("password");
        if (db.count("name")) db_config_.database = db.at("name");
        if (db.count("charset")) db_config_.charset = db.at("charset");
        if (db.count("collation")) db_config_.collation = db.at("collation");
    }

    if (sections.find("server") != sections.end()) {
        const auto& srv = sections["server"];
        if (srv.count("host")) server_config_.host = srv.at("host");
        if (srv.count("port")) server_config_.port = std::stoi(srv.at("port"));
    }

    if (sections.find("app") != sections.end()) {
        const auto& app = sections["app"];
        if (app.count("test_case_dir")) app_config_.test_case_dir = app.at("test_case_dir");
        if (app.count("upload_dir")) app_config_.upload_dir = app.at("upload_dir");
        if (app.count("log_level")) app_config_.log_level = app.at("log_level");
        if (app.count("log_file")) app_config_.log_file = app.at("log_file");
    }

    return true;
}