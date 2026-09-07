#pragma once

#include "httplib.h"

void handleRegister(const httplib::Request& req, httplib::Response& res);
void handleLogin(const httplib::Request& req, httplib::Response& res);
void handleLogout(const httplib::Request& req, httplib::Response& res);
void handleGetCurrentUser(const httplib::Request& req, httplib::Response& res);
void handleDeleteAccount(const httplib::Request& req, httplib::Response& res);
