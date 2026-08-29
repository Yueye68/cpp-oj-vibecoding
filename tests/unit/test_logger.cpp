#include <gtest/gtest.h>
#include "utils/logger.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#include <sstream>

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempFile_ = "/tmp/test_logger_" + std::to_string(::getpid()) + ".log";
    }

    void TearDown() override {
        Logger::instance().setLevel(LogLevel::INFO);
        std::filesystem::remove(tempFile_);
    }

    std::string tempFile_;
};

TEST_F(LoggerTest, Singleton_Persistence) {
    Logger& logger1 = Logger::instance();
    logger1.info("test message");

    Logger& logger2 = Logger::instance();
    EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggerTest, LogLevel_FilterByLevel) {
    Logger& logger = Logger::instance();

    logger.setLevel(LogLevel::ERROR);
    logger.debug("debug_message");
    logger.info("info_message");
    logger.warn("warn_message");
    logger.error("error_message");
}

TEST_F(LoggerTest, LogLevel_ChangeLevel) {
    Logger& logger = Logger::instance();

    logger.setLevel(LogLevel::DEBUG);
    EXPECT_NO_FATAL_FAILURE(logger.debug("debug after set DEBUG"));

    logger.setLevel(LogLevel::WARN);
    EXPECT_NO_FATAL_FAILURE(logger.warn("warn after set WARN"));
}

TEST_F(LoggerTest, LogLevel_AllLevels) {
    Logger& logger = Logger::instance();
    logger.setLevel(LogLevel::DEBUG);

    EXPECT_NO_FATAL_FAILURE(logger.debug("debug message"));
    EXPECT_NO_FATAL_FAILURE(logger.info("info message"));
    EXPECT_NO_FATAL_FAILURE(logger.warn("warn message"));
    EXPECT_NO_FATAL_FAILURE(logger.error("error message"));
}

TEST_F(LoggerTest, FileOutput_BasicWrite) {
    Logger& logger = Logger::instance();
    logger.setFile(tempFile_);

    logger.info("test log message");

    std::ifstream file(tempFile_);
    ASSERT_TRUE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_TRUE(content.find("test log message") != std::string::npos);
}

TEST_F(LoggerTest, FileOutput_MultipleLogs) {
    Logger& logger = Logger::instance();
    logger.setFile(tempFile_);

    logger.info("first message");
    logger.warn("second message");
    logger.error("third message");

    std::ifstream file(tempFile_);
    ASSERT_TRUE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_TRUE(content.find("first message") != std::string::npos);
    EXPECT_TRUE(content.find("second message") != std::string::npos);
    EXPECT_TRUE(content.find("third message") != std::string::npos);
}

TEST_F(LoggerTest, FileOutput_AppendMode) {
    Logger& logger = Logger::instance();
    logger.setFile(tempFile_);

    logger.info("first batch");
    logger.setFile(tempFile_);
    logger.info("second batch");

    std::ifstream file(tempFile_);
    ASSERT_TRUE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_TRUE(content.find("first batch") != std::string::npos);
    EXPECT_TRUE(content.find("second batch") != std::string::npos);
}

TEST_F(LoggerTest, FileOutput_LevelFiltering) {
    Logger& logger = Logger::instance();
    logger.setLevel(LogLevel::WARN);
    logger.setFile(tempFile_);

    logger.debug("debug should not appear");
    logger.info("info should not appear");
    logger.warn("warn should appear");
    logger.error("error should appear");

    std::ifstream file(tempFile_);
    ASSERT_TRUE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_TRUE(content.find("warn should appear") != std::string::npos);
    EXPECT_TRUE(content.find("error should appear") != std::string::npos);
    EXPECT_FALSE(content.find("debug should not appear") != std::string::npos);
}

TEST_F(LoggerTest, FileOutput_InvalidPath) {
    Logger& logger = Logger::instance();
    EXPECT_NO_FATAL_FAILURE(logger.setFile("/nonexistent/path/that/does/not/exist.log"));
}

TEST_F(LoggerTest, ConcurrentWrite) {
    Logger& logger = Logger::instance();
    logger.setLevel(LogLevel::DEBUG);
    logger.setFile(tempFile_);

    const int numThreads = 4;
    const int logsPerThread = 50;
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&logger, t, logsPerThread]() {
            for (int i = 0; i < logsPerThread; ++i) {
                logger.info("thread " + std::to_string(t) + " message " + std::to_string(i));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::ifstream file(tempFile_);
    ASSERT_TRUE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    int lineCount = 0;
    std::string line;
    std::istringstream stream(content);
    while (std::getline(stream, line)) {
        if (!line.empty()) lineCount++;
    }

    EXPECT_EQ(lineCount, numThreads * logsPerThread);
}
