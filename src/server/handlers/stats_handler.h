#pragma once

#include <httplib.h>

void handleGetStats(const httplib::Request& req, httplib::Response& res);
