#pragma once

#include "httplib.h"

void handleListProblems(const httplib::Request& req, httplib::Response& res);
void handleGetProblem(const httplib::Request& req, httplib::Response& res);
void handleCreateProblem(const httplib::Request& req, httplib::Response& res);
void handleUpdateProblem(const httplib::Request& req, httplib::Response& res);
void handleDeleteProblem(const httplib::Request& req, httplib::Response& res);
void handleListProblemTags(const httplib::Request& req, httplib::Response& res);
