#pragma once

#include <string>
#include <source_location>
#include <fstream>
#include <mutex>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& instance();

    void log(LogLevel level, const std::string& message,
             const std::source_location& loc = std::source_location::current());

    void debug(const std::string& message,
               const std::source_location& loc = std::source_location::current());
    void info(const std::string& message,
              const std::source_location& loc = std::source_location::current());
    void warn(const std::string& message,
              const std::source_location& loc = std::source_location::current());
    void error(const std::string& message,
               const std::source_location& loc = std::source_location::current());

    void setLevel(LogLevel level);
    void setFile(const std::string& filepath);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void writeLog(LogLevel level, const std::string& formatted);

    LogLevel minLevel_ = LogLevel::INFO;
    std::ofstream fileStream_;
    std::mutex mutex_;
    bool fileEnabled_ = false;
};
