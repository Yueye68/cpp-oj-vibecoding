#include "judge_worker.h"
#include "logger.h"
#include "config.h"
#include "connection_pool.h"
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

static std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Config& config = Config::instance();
    std::string configPath = "./config/config.yaml";
    if (argc > 1) {
        configPath = argv[1];
    }

    if (!config.load(configPath)) {
        std::cerr << "Failed to load config from " << configPath << std::endl;
        return 1;
    }

    Logger::instance().info("Starting Judge Worker");
    Logger::instance().info("Log level: " + config.app().log_level);

    ConnectionPool::instance().init(config.database(), 5);

    int workerThreads = config.worker().worker_threads;
    std::string workerIdPrefix = config.worker().worker_id_prefix;

    std::vector<std::unique_ptr<JudgeWorker>> workers;

    Logger::instance().info("Starting " + std::to_string(workerThreads) + " worker threads...");

    for (int i = 0; i < workerThreads; ++i) {
        std::string workerId = workerIdPrefix + "_" + std::to_string(i);
        auto worker = std::make_unique<JudgeWorker>(workerId);
        worker->start();
        workers.push_back(std::move(worker));
    }

    Logger::instance().info("All workers started. Press Ctrl+C to stop.");

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    Logger::instance().info("Shutting down workers...");

    for (auto& worker : workers) {
        worker->stop();
    }

    ConnectionPool::instance().close();
    Logger::instance().info("Workers stopped. Exiting.");

    return 0;
}