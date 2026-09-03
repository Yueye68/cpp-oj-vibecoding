#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mysql/mysql.h>
#include "connection_pool.h"

struct Submission {
    int id = 0;
    int user_id = 0;
    int problem_id = 0;
    std::string code;
    std::string language = "cpp";
    std::string status;
    std::string queue_status = "pending";
    std::string error_detail;
    int execute_time_ms = 0;
    int execute_memory_kb = 0;
    std::string created_at;

    bool create();
    bool update();
    bool remove();
    bool loadFromDB(int loadId);
    bool saveToDB();
    bool updateQueueStatus(const std::string& qs);

    static std::optional<Submission> findById(int id);
    static std::vector<Submission> findAll(int page = 1, int pageSize = 20);
    static std::vector<Submission> findByUserId(int userId, int page = 1, int pageSize = 20);
    static std::vector<Submission> findByProblemId(int problemId, int page = 1, int pageSize = 20);
    static int countAll();
    static int countByUserId(int userId);
    static int countByProblemId(int problemId);
};