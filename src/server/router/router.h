#pragma once

#include "httplib.h"
#include "server.h"
#include <functional>
#include <string>

using Handler = std::function<void(const httplib::Request&, httplib::Response&)>;

class Router {
public:
    static Router& instance();

    void registerHandlers();

    httplib::Server& getServer() { return svr_; }

private:
    Router() = default;
    ~Router() = default;
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    void registerAuthRoutes();
    void registerProblemRoutes();
    void registerTestCaseRoutes();
    void registerSubmissionRoutes();
    void registerStatsRoutes();

    httplib::Server& svr_ = Server::instance().getServer();
};
