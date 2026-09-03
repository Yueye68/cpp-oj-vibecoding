#include "server.h"
#include "logger.h"
#include <csignal>
#include <atomic>

extern std::atomic<bool> g_running;

Server& Server::instance() {
    static Server instance;
    return instance;
}

void Server::init(const std::string& host, int port) {
    host_ = host;
    port_ = port;
    Logger::instance().info("Server initialized on " + host + ":" + std::to_string(port));
}

void Server::start() {
    running_ = true;
    Logger::instance().info("Server starting on " + host_ + ":" + std::to_string(port_));

    svr_.set_mount_point("/", "./web");

    svr_.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });

    svr_.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    });

    svr_.set_exception_handler([](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
        try {
            if (ep) {
                std::rethrow_exception(ep);
            }
        } catch (const std::exception& e) {
            Logger::instance().error("Exception: " + std::string(e.what()));
        } catch (...) {
            Logger::instance().error("Unknown exception");
        }
        res.status = 500;
        res.set_content(R"({"error": "Internal server error"})", "application/json");
    });

    svr_.listen(host_.c_str(), port_);
}

void Server::stop() {
    running_ = false;
    svr_.stop();
    Logger::instance().info("Server stopped");
}
