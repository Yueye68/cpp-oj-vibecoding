#include "submission_handler.h"
#include "auth_middleware.h"
#include "logger.h"
#include "submission.h"
#include "submission_result.h"
#include "problem.h"
#include "test_case.h"
#include "judge_queue.h"
#include "json.h"
#include <sstream>

static bool parseJson(const std::string& body, Json::Value& json) {
    std::istringstream iss(body);
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    Json::String err;
    return Json::parseFromStream(builder, iss, &json, &err);
}

void handleCreateSubmission(const httplib::Request& req, httplib::Response& res) {
    if (!AuthMiddleware::requireAuth(req, res)) {
        return;
    }

    auto userOpt = AuthMiddleware::getCurrentUser(req);
    if (!userOpt.has_value()) {
        res.status = 401;
        res.set_content("{\"error\": \"Unauthorized\"}", "application/json");
        return;
    }

    Json::Value json;
    if (!parseJson(req.body, json)) {
        res.status = 400;
        res.set_content("{\"error\": \"Invalid JSON\"}", "application/json");
        return;
    }

    if (!json["problem_id"].isInt() || !json["code"].isString() || !json["language"].isString()) {
        res.status = 400;
        res.set_content("{\"error\": \"Missing required fields\"}", "application/json");
        return;
    }

    int problemId = json["problem_id"].asInt();
    auto problemOpt = Problem::findById(problemId);
    if (!problemOpt.has_value()) {
        res.status = 404;
        res.set_content("{\"error\": \"Problem not found\"}", "application/json");
        return;
    }

    Submission submission;
    submission.user_id = userOpt.value().id;
    submission.problem_id = problemId;
    submission.code = json["code"].asString();
    submission.language = json["language"].asString();
    submission.status = "pending";
    submission.queue_status = "pending";

    if (!submission.create()) {
        Logger::instance().error("Failed to create submission");
        res.status = 500;
        res.set_content("{\"error\": \"Failed to create submission\"}", "application/json");
        return;
    }

    Logger::instance().info("Submission created: ID " + std::to_string(submission.id) + ", problem: " + std::to_string(problemId));

    JudgeQueueItem queueItem;
    queueItem.submission_id = submission.id;
    queueItem.priority = 0;
    if (!queueItem.create()) {
        Logger::instance().error("Failed to enqueue submission: " + std::to_string(submission.id));
    }

    Json::Value result;
    result["message"] = "Submission created and queued for judging";
    result["id"] = submission.id;
    result["status"] = "pending";
    result["queue_status"] = "pending";

    res.status = 201;
    res.set_content(result.toStyledString(), "application/json");
}

void handleListSubmissions(const httplib::Request& req, httplib::Response& res) {
    if (!AuthMiddleware::requireAuth(req, res)) {
        return;
    }

    auto userOpt = AuthMiddleware::getCurrentUser(req);
    if (!userOpt.has_value()) {
        res.status = 401;
        res.set_content("{\"error\": \"Unauthorized\"}", "application/json");
        return;
    }

    int page = 1;
    int pageSize = 20;

    if (auto it = req.params.find("page"); it != req.params.end()) {
        page = std::stoi(it->second);
        if (page < 1) page = 1;
    }
    if (auto it = req.params.find("pageSize"); it != req.params.end()) {
        pageSize = std::stoi(it->second);
        if (pageSize < 1) pageSize = 20;
        if (pageSize > 100) pageSize = 100;
    }

    User user = userOpt.value();
    std::vector<Submission> submissions;
    int total = 0;

    if (user.role == UserRole::Admin) {
        submissions = Submission::findAll(page, pageSize);
        total = Submission::countAll();
    } else {
        submissions = Submission::findByUserId(user.id, page, pageSize);
        total = Submission::countByUserId(user.id);
    }

    Json::Value result;
    Json::Value submissionsArray(Json::arrayValue);
    for (const auto& s : submissions) {
        Json::Value js;
        js["id"] = s.id;
        js["user_id"] = s.user_id;
        js["problem_id"] = s.problem_id;
        js["problem_title"] = s.problem_title;
        js["language"] = s.language;
        js["status"] = s.status;
        js["queue_status"] = s.queue_status;
        js["execute_time_ms"] = s.execute_time_ms;
        js["execute_memory_kb"] = s.execute_memory_kb;
        js["created_at"] = s.created_at;
        submissionsArray.append(js);
    }

    result["submissions"] = submissionsArray;
    result["total"] = total;
    result["page"] = page;
    result["pageSize"] = pageSize;
    result["totalPages"] = (total + pageSize - 1) / pageSize;

    res.status = 200;
    res.set_content(result.toStyledString(), "application/json");
}

void handleGetSubmission(const httplib::Request& req, httplib::Response& res) {
    if (!AuthMiddleware::requireAuth(req, res)) {
        return;
    }

    auto userOpt = AuthMiddleware::getCurrentUser(req);
    if (!userOpt.has_value()) {
        res.status = 401;
        res.set_content("{\"error\": \"Unauthorized\"}", "application/json");
        return;
    }

    int id = std::stoi(req.path_params.at("id"));

    auto submissionOpt = Submission::findById(id);
    if (!submissionOpt.has_value()) {
        res.status = 404;
        res.set_content("{\"error\": \"Submission not found\"}", "application/json");
        return;
    }

    Submission submission = submissionOpt.value();
    User user = userOpt.value();

    if (user.role != UserRole::Admin && submission.user_id != user.id) {
        res.status = 403;
        res.set_content("{\"error\": \"Forbidden\"}", "application/json");
        return;
    }

    Json::Value js;
    js["id"] = submission.id;
    js["user_id"] = submission.user_id;
    js["problem_id"] = submission.problem_id;
    js["code"] = submission.code;
    js["language"] = submission.language;
    js["status"] = submission.status;
    js["queue_status"] = submission.queue_status;
    js["error_detail"] = submission.error_detail;
    js["execute_time_ms"] = submission.execute_time_ms;
    js["execute_memory_kb"] = submission.execute_memory_kb;
    js["created_at"] = submission.created_at;

    auto results = SubmissionResult::findBySubmissionId(submission.id);
    Json::Value resultsArray(Json::arrayValue);
    for (const auto& sr : results) {
        Json::Value srJson;
        srJson["id"] = sr.id;
        srJson["test_case_id"] = sr.test_case_id;
        srJson["status"] = sr.status;
        srJson["actual_output"] = sr.actual_output;
        srJson["expected_output"] = sr.expected_output;
        srJson["execute_time_ms"] = sr.execute_time_ms;
        srJson["execute_memory_kb"] = sr.execute_memory_kb;
        resultsArray.append(srJson);
    }
    js["results"] = resultsArray;

    res.status = 200;
    res.set_content(js.toStyledString(), "application/json");
}