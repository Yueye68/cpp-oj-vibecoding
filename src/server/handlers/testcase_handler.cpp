#include "testcase_handler.h"
#include "auth_middleware.h"
#include "logger.h"
#include "test_case.h"
#include "problem.h"
#include "config.h"
#include "json.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static bool parseJson(const std::string& body, Json::Value& json) {
    std::istringstream iss(body);
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    Json::String err;
    return Json::parseFromStream(builder, iss, &json, &err);
}

void handleAddTestCase(const httplib::Request& req, httplib::Response& res) {
    if (!AuthMiddleware::requireAdmin(req, res)) {
        return;
    }

    int problemId = std::stoi(req.path_params.at("id"));

    auto problemOpt = Problem::findById(problemId);
    if (!problemOpt.has_value()) {
        res.status = 404;
        res.set_content("{\"error\": \"Problem not found\"}", "application/json");
        return;
    }

    if (!req.form.has_file("input") || !req.form.has_file("output")) {
        res.status = 400;
        res.set_content("{\"error\": \"Both input and output files are required\"}", "application/json");
        return;
    }

    const auto& inputFile = req.form.get_file("input");
    const auto& outputFile = req.form.get_file("output");

    size_t maxFileSize = Config::instance().app().max_test_case_file_size;
    if (inputFile.content.size() > maxFileSize) {
        res.status = 400;
        res.set_content("{\"error\": \"Input file exceeds maximum size limit (10MB)\"}", "application/json");
        return;
    }
    if (outputFile.content.size() > maxFileSize) {
        res.status = 400;
        res.set_content("{\"error\": \"Output file exceeds maximum size limit (10MB)\"}", "application/json");
        return;
    }

    int currentCount = TestCase::countByProblemId(problemId);
    if (currentCount >= Config::instance().app().max_test_case_count) {
        res.status = 400;
        res.set_content("{\"error\": \"Maximum test case count (20) reached for this problem\"}", "application/json");
        return;
    }

    std::string testCaseDir = Config::instance().app().test_case_dir + "/" + std::to_string(problemId);
    if (!fs::exists(testCaseDir)) {
        fs::create_directories(testCaseDir);
    }

    std::string inputFilename = std::to_string(time(nullptr)) + "_in.txt";
    std::string outputFilename = std::to_string(time(nullptr)) + "_out.txt";
    std::string inputPath = testCaseDir + "/" + inputFilename;
    std::string outputPath = testCaseDir + "/" + outputFilename;

    std::ofstream inputOut(inputPath, std::ios::binary);
    inputOut.write(inputFile.content.data(), inputFile.content.size());
    inputOut.close();

    std::ofstream outputOut(outputPath, std::ios::binary);
    outputOut.write(outputFile.content.data(), outputFile.content.size());
    outputOut.close();

    TestCase tc;
    tc.problem_id = problemId;
    tc.input_path = inputPath;
    tc.output_path = outputPath;
    tc.score = 100;
    tc.is_sample = false;

    bool isSample = req.params.find("is_sample") != req.params.end() && req.params.find("is_sample")->second == "true";
    if (isSample) {
        tc.is_sample = true;
    }

    if (!tc.create()) {
        Logger::instance().error("Failed to create test case for problem: " + std::to_string(problemId));
        res.status = 500;
        res.set_content("{\"error\": \"Failed to save test case\"}", "application/json");
        return;
    }

    Logger::instance().info("Test case added for problem: " + std::to_string(problemId));

    Json::Value result;
    result["message"] = "Test case added successfully";
    result["id"] = tc.id;
    result["input_path"] = tc.input_path;
    result["output_path"] = tc.output_path;
    result["is_sample"] = tc.is_sample;

    res.status = 201;
    res.set_content(result.toStyledString(), "application/json");
}

void handleDeleteTestCase(const httplib::Request& req, httplib::Response& res) {
    if (!AuthMiddleware::requireAdmin(req, res)) {
        return;
    }

    int testCaseId = std::stoi(req.path_params.at("id"));

    auto tcOpt = TestCase::findById(testCaseId);
    if (!tcOpt.has_value()) {
        res.status = 404;
        res.set_content("{\"error\": \"Test case not found\"}", "application/json");
        return;
    }

    TestCase tc = tcOpt.value();

    if (fs::exists(tc.input_path)) {
        fs::remove(tc.input_path);
    }
    if (fs::exists(tc.output_path)) {
        fs::remove(tc.output_path);
    }

    if (!tc.remove()) {
        Logger::instance().error("Failed to delete test case: " + std::to_string(testCaseId));
        res.status = 500;
        res.set_content("{\"error\": \"Failed to delete test case\"}", "application/json");
        return;
    }

    Logger::instance().info("Test case deleted: " + std::to_string(testCaseId));
    res.status = 200;
    res.set_content("{\"message\": \"Test case deleted successfully\"}", "application/json");
}

void handleGetTestCasesByProblem(const httplib::Request& req, httplib::Response& res) {
    int problemId = std::stoi(req.path_params.at("id"));

    auto problemOpt = Problem::findById(problemId);
    if (!problemOpt.has_value()) {
        res.status = 404;
        res.set_content("{\"error\": \"Problem not found\"}", "application/json");
        return;
    }

    auto testCases = TestCase::findByProblemId(problemId);

    Json::Value result;
    Json::Value testCasesArray(Json::arrayValue);
    for (const auto& tc : testCases) {
        Json::Value jtc;
        jtc["id"] = tc.id;
        jtc["problem_id"] = tc.problem_id;
        jtc["score"] = tc.score;
        jtc["is_sample"] = tc.is_sample;
        testCasesArray.append(jtc);
    }
    result["test_cases"] = testCasesArray;
    result["count"] = static_cast<int>(testCases.size());

    res.status = 200;
    res.set_content(result.toStyledString(), "application/json");
}