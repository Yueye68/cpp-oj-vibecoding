#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mysql/mysql.h>
#include "connection_pool.h"

struct TestCase {
    int id = 0;
    int problem_id = 0;
    std::string input_path;
    std::string output_path;
    int score = 100;
    bool is_sample = false;

    bool create();
    bool update();
    bool remove();
    bool loadFromDB(int loadId);
    bool saveToDB();

    static std::optional<TestCase> findById(int id);
    static std::vector<TestCase> findByProblemId(int problemId);
    static int countByProblemId(int problemId);
};