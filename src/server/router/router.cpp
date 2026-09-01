#include "router.h"
#include "handlers/auth_handler.h"
#include "handlers/problem_handler.h"
#include "logger.h"

Router& Router::instance() {
    static Router instance;
    return instance;
}

void Router::registerHandlers() {
    Logger::instance().info("Registering route handlers");
    registerAuthRoutes();
    registerProblemRoutes();
    Logger::instance().info("All routes registered");
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
