#pragma once

#include "httplib.h"

void handleAddTestCase(const httplib::Request& req, httplib::Response& res);
void handleDeleteTestCase(const httplib::Request& req, httplib::Response& res);
void handleGetTestCasesByProblem(const httplib::Request& req, httplib::Response& res);