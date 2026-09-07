#include "router.h"
#include "handlers/auth_handler.h"
#include "handlers/problem_handler.h"
#include "handlers/testcase_handler.h"
#include "handlers/submission_handler.h"
#include "handlers/stats_handler.h"
#include "logger.h"

Router& Router::instance() {
    static Router instance;
    return instance;
}

void Router::registerHandlers() {
    Logger::instance().info("Registering route handlers");
    registerAuthRoutes();
    registerProblemRoutes();
    registerTestCaseRoutes();
    registerSubmissionRoutes();
    registerStatsRoutes();
    Logger::instance().info("All routes registered");
}

void Router::registerStatsRoutes() {
    svr_.Get("/api/stats", handleGetStats);
}

void Router::registerAuthRoutes() {
    svr_.Post("/api/auth/register", handleRegister);
    svr_.Post("/api/auth/login", handleLogin);
    svr_.Post("/api/auth/logout", handleLogout);
    svr_.Get("/api/auth/me", handleGetCurrentUser);
}

void Router::registerProblemRoutes() {
    svr_.Get("/api/problems", handleListProblems);
    svr_.Get("/api/problems/:id", handleGetProblem);
    svr_.Post("/api/problems", handleCreateProblem);
    svr_.Put("/api/problems/:id", handleUpdateProblem);
    svr_.Delete("/api/problems/:id", handleDeleteProblem);
}

void Router::registerTestCaseRoutes() {
    svr_.Post("/api/problems/:id/testcases", handleAddTestCase);
    svr_.Get("/api/problems/:id/testcases", handleGetTestCasesByProblem);
    svr_.Delete("/api/testcases/:id", handleDeleteTestCase);
}

void Router::registerSubmissionRoutes() {
    svr_.Post("/api/submissions", handleCreateSubmission);
    svr_.Get("/api/submissions", handleListSubmissions);
    svr_.Get("/api/submissions/:id", handleGetSubmission);
}
