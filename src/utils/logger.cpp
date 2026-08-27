#include "logger.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::log(LogLevel level, const std::string& message,
                 const std::source_location& loc) {
    std::ostringstream oss;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    localtime_r(&time_t, &tm_buf);

    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();

    std::string levelStr;
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO:  levelStr = "INFO";  break;
        case LogLevel::WARN:  levelStr = "WARN";  break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
    }

    oss << " [" << levelStr << "] "
        << message
        << " (" << loc.file_name() << ":" << loc.line() << ")";

    writeLog(level, oss.str());
}

void Logger::writeLog(LogLevel level, const std::string& formatted) {
    std::cout << formatted << std::endl;
}

void Logger::debug(const std::string& message, const std::source_location& loc) {
    log(LogLevel::DEBUG, message, loc);
}

void Logger::info(const std::string& message, const std::source_location& loc) {
    log(LogLevel::INFO, message, loc);
}

void Logger::warn(const std::string& message, const std::source_location& loc) {
    log(LogLevel::WARN, message, loc);
}

void Logger::error(const std::string& message, const std::source_location& loc) {
    log(LogLevel::ERROR, message, loc);
}

void Logger::setLevel(LogLevel level) {
}

void Logger::setFile(const std::string& filepath) {
}
