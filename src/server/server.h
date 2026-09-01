#pragma once

#include "httplib.h"
#include <string>

class Server {
public:
    static Server& instance();

    void init(const std::string& host, int port);
    void start();
    void stop();

    httplib::Server& getServer() { return svr_; }

private:
    Server() = default;
    ~Server() = default;
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    httplib::Server svr_;
    std::string host_;
    int port_ = 8080;
    bool running_ = false;
};
