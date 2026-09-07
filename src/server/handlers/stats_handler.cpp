#include "stats_handler.h"

#include "json.h"

#include "user.h"
#include "problem.h"
#include "submission.h"
#include "logger.h"

void handleGetStats(const httplib::Request& req, httplib::Response& res) {
    (void)req;

    int userCount = User::count();
    int problemCount = Problem::count();
    int submissionCount = Submission::countAll();

    Json::Value response;
    response["users"] = userCount;
    response["problems"] = problemCount;
    response["submissions"] = submissionCount;

    res.set_content(response.toStyledString(), "application/json");
}
