#include <iostream>
#include <csignal>
#include <atomic>
#include "utils/config.h"
#include "utils/logger.h"
#include "db_pool/connection_pool.h"
#include "server/server.h"
#include "server/router/router.h"

std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
        Server::instance().stop();
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Config& config = Config::instance();
    if (!config.load("config/config.yaml")) {
        std::cerr << "Failed to load config file" << std::endl;
        return 1;
    }

    Logger::instance().info("Starting OJ Server");

    ConnectionPool::instance().init(config.database(), 10);

    Server& server = Server::instance();
    server.init(config.server().host, config.server().port);

    Router::instance().registerHandlers();

    Logger::instance().info("Database: " + config.database().host + ":" + std::to_string(config.database().port));
    Logger::instance().info("Server: " + config.server().host + ":" + std::to_string(config.server().port));

    server.start();

    ConnectionPool::instance().close();
    Logger::instance().info("Server shutdown complete");

    return 0;
}
