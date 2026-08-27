#include <iostream>
#include "utils/config.h"
#include "utils/logger.h"

int main(int argc, char* argv[]) {
    Config& config = Config::instance();
    if (!config.load("config/config.yaml")) {
        std::cerr << "Failed to load config file" << std::endl;
        return 1;
    }

    Logger::instance().info("Starting OJ Server");

    std::cout << "Database: " << config.database().host << ":" << config.database().port << std::endl;
    std::cout << "Server: " << config.server().host << ":" << config.server().port << std::endl;

    return 0;
}
