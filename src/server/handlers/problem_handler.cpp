#include "problem_handler.h"
#include "auth_middleware.h"
#include "logger.h"
#include "problem.h"
#include "json.h"
#include <sstream>

static bool parseJson(const std::string& body, Json::Value& json) {
    std::istringstream iss(body);
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    Json::String err;
    return Json::parseFromStream(builder, iss, &json, &err);
}

void handleListProblems(const httplib::Request& req, httplib::Response& res) {
    res.status = 200;
    int page = 1;
    int pageSize = 20;
    std::string difficulty;
    std::string search;

    if (auto it = req.params.find("page"); it != req.params.end()) {
        page = std::stoi(it->second);
        if (page < 1) page = 1;
    }
    if (auto it = req.params.find("pageSize"); it != req.params.end()) {
        pageSize = std::stoi(it->second);
        if (pageSize < 1) pageSize = 20;
        if (pageSize > 100) pageSize = 100;
    }
    if (auto it = req.params.find("difficulty"); it != req.params.end()) {
        difficulty = it->second;
    }
    if (auto it = req.params.find("search"); it != req.params.end()) {
        search = it->second;
    }

    std::vector<std::string> tags;
    if (auto it = req.params.find("tags"); it != req.params.end()) {
        std::string tagsStr = it->second;
        std::istringstream iss(tagsStr);
        std::string tag;
        while (std::getline(iss, tag, ',')) {
            tags.push_back(tag);
        }
    }

    auto problems = Problem::findAll(page, pageSize, difficulty, tags, search);
    int total = Problem::count(difficulty, tags, search);

    Json::Value result;
    Json::Value problemsArray(Json::arrayValue);
    for (const auto& p : problems) {
        Json::Value jp;
        jp["id"] = p.id;
        jp["title"] = p.title;
        jp["difficulty"] = Problem::difficultyToString(p.difficulty);
        Json::Value tagsArray(Json::arrayValue);
        for (const auto& tag : p.tags) {
            tagsArray.append(tag);
        }
        jp["tags"] = tagsArray;
        jp["time_limit_ms"] = p.time_limit_ms;
        jp["memory_limit_mb"] = p.memory_limit_mb;
        jp["test_case_count"] = p.test_case_count;
        jp["created_at"] = p.created_at;
        problemsArray.append(jp);
    }
    result["problems"] = problemsArray;
    result["total"] = total;
    result["page"] = page;
    result["pageSize"] = pageSize;
    result["totalPages"] = (total + pageSize - 1) / pageSize;

    res.status = 200;
    res.set_content(result.toStyledString(), "application/json");
}

void handleGetProblem(const httplib::Request& req, httplib::Response& res) {
    res.status = 200;
    int id = std::stoi(req.path_params.at("id"));

    auto problemOpt = Problem::findById(id);
    if (!problemOpt.has_value()) {
        res.status = 404;
        res.set_content("{\"error\": \"Problem not found\"}", "application/json");
        return;
    }

    const Problem& p = problemOpt.value();
    Json::Value jp;
    jp["id"] = p.id;
    jp["title"] = p.title;
    jp["description"] = p.description;
    jp["difficulty"] = Problem::difficultyToString(p.difficulty);
    Json::Value tagsArray(Json::arrayValue);
    for (const auto& tag : p.tags) {
        tagsArray.append(tag);
    }
    jp["tags"] = tagsArray;
    jp["time_limit_ms"] = p.time_limit_ms;
    jp["memory_limit_mb"] = p.memory_limit_mb;
    jp["test_case_count"] = p.test_case_count;
    jp["created_at"] = p.created_at;
    jp["updated_at"] = p.updated_at;

    res.set_content(jp.toStyledString(), "application/json");
}

void handleCreateProblem(const httplib::Request& req, httplib::Response& res) {
    if (!AuthMiddleware::requireAdmin(req, res)) {
        return;
    }

    Json::Value json;
    if (!parseJson(req.body, json)) {
        res.status = 400;
        res.set_content("{\"error\": \"Invalid JSON\"}", "application/json");
        return;
    }

    std::string title = json["title"].asString();
    std::string description = json["description"].asString();
    std::string difficultyStr = json["difficulty"].asString();

    if (title.empty()) {
        res.status = 400;
        res.set_content("{\"error\": \"Title is required\"}", "application/json");
        return;
    }

    Problem problem;
    problem.title = title;
    problem.description = description;
    problem.difficulty = Problem::stringToDifficulty(difficultyStr);
    problem.time_limit_ms = json["time_limit_ms"].asInt();
    problem.memory_limit_mb = json["memory_limit_mb"].asInt();

    if (json["tags"].isArray()) {
        for (const auto& tag : json["tags"]) {
            problem.tags.push_back(tag.asString());
        }
    }

    if (!problem.create()) {
        Logger::instance().error("Failed to create problem: " + title);
        res.status = 500;
        res.set_content("{\"error\": \"Failed to create problem\"}", "application/json");
        return;
    }

    Logger::instance().info("Problem created: " + title + " (ID: " + std::to_string(problem.id) + ")");
    res.status = 201;
    Json::Value result;
    result["message"] = "Problem created successfully";
    result["id"] = problem.id;
    res.set_content(result.toStyledString(), "application/json");
}

void handleUpdateProblem(const httplib::Request& req, httplib::Response& res) {
    if (!AuthMiddleware::requireAdmin(req, res)) {
        return;
    }

    int id = std::stoi(req.path_params.at("id"));

    auto problemOpt = Problem::findById(id);
    if (!problemOpt.has_value()) {
        res.status = 404;
        res.set_content("{\"error\": \"Problem not found\"}", "application/json");
        return;
    }

    Problem problem = problemOpt.value();

    Json::Value json;
    if (!parseJson(req.body, json)) {
        res.status = 400;
        res.set_content("{\"error\": \"Invalid JSON\"}", "application/json");
        return;
    }

    if (json["title"].isString()) {
        problem.title = json["title"].asString();
    }
    if (json["description"].isString()) {
        problem.description = json["description"].asString();
    }
    if (json["difficulty"].isString()) {
        problem.difficulty = Problem::stringToDifficulty(json["difficulty"].asString());
    }
    if (json["time_limit_ms"].isInt()) {
        problem.time_limit_ms = json["time_limit_ms"].asInt();
    }
    if (json["memory_limit_mb"].isInt()) {
        problem.memory_limit_mb = json["memory_limit_mb"].asInt();
    }
    if (json["tags"].isArray()) {
        problem.tags.clear();
        for (const auto& tag : json["tags"]) {
            problem.tags.push_back(tag.asString());
        }
    }

    if (!problem.update()) {
        Logger::instance().error("Failed to update problem ID: " + std::to_string(id));
        res.status = 500;
        res.set_content("{\"error\": \"Failed to update problem\"}", "application/json");
        return;
    }

    Logger::instance().info("Problem updated: ID " + std::to_string(id));
    res.status = 200;
    res.set_content("{\"message\": \"Problem updated successfully\"}", "application/json");
}

void handleDeleteProblem(const httplib::Request& req, httplib::Response& res) {
    if (!AuthMiddleware::requireAdmin(req, res)) {
        return;
    }

    int id = std::stoi(req.path_params.at("id"));

    auto problemOpt = Problem::findById(id);
    if (!problemOpt.has_value()) {
        res.status = 404;
        res.set_content("{\"error\": \"Problem not found\"}", "application/json");
        return;
    }

    Problem problem = problemOpt.value();
    problem.id = id;

    if (!problem.remove()) {
        Logger::instance().error("Failed to delete problem ID: " + std::to_string(id));
        res.status = 500;
        res.set_content("{\"error\": \"Failed to delete problem\"}", "application/json");
        return;
    }

    Logger::instance().info("Problem deleted: ID " + std::to_string(id));
    res.status = 200;
    res.set_content("{\"message\": \"Problem deleted successfully\"}", "application/json");
}

void handleListProblemTags(const httplib::Request& req, httplib::Response& res) {
    (void)req;
    auto tagCounts = Problem::listTagsWithCount();

    Json::Value response(Json::objectValue);
    Json::Value arr(Json::arrayValue);
    for (const auto& [name, count] : tagCounts) {
        Json::Value item;
        item["name"] = name;
        item["count"] = count;
        arr.append(item);
    }
    response["tags"] = arr;
    res.set_content(response.toStyledString(), "application/json");
}
