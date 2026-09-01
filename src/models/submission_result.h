#pragma once

#include <string>
#include <optional>
#include <mysql/mysql.h>
#include "connection_pool.h"

struct SubmissionResult {
    int id = 0;
    int submission_id = 0;
    int test_case_id = 0;
    std::string status;
    std::string actual_output;
    std::string expected_output;
    int execute_time_ms = 0;
    int execute_memory_kb = 0;

    bool create();
    bool update();
    bool remove();
    bool loadFromDB(int loadId);
    bool saveToDB();

    static std::optional<SubmissionResult> findById(int id);
    static std::vector<SubmissionResult> findBySubmissionId(int submissionId);
};