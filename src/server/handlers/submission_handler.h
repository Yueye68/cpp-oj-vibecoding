#pragma once

#include "httplib.h"

void handleCreateSubmission(const httplib::Request& req, httplib::Response& res);
void handleListSubmissions(const httplib::Request& req, httplib::Response& res);
void handleGetSubmission(const httplib::Request& req, httplib::Response& res);