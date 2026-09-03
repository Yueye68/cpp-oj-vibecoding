#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mysql/mysql.h>
#include "connection_pool.h"

struct JudgeQueueItem {
    int id = 0;
    int submission_id = 0;
    int priority = 0;
    std::string status;
    std::string worker_id;
    int retry_count = 0;
    std::string error_message;
    std::string created_at;
    std::string started_at;
    std::string completed_at;

    bool create();
    bool update();
    bool markRunning(const std::string& workerId);
    bool markCompleted();
    bool markFailed(const std::string& error);

    static std::optional<JudgeQueueItem> popPending(const std::string& workerId);
    static std::vector<JudgeQueueItem> findBySubmissionId(int submissionId);
    static int countPending();
};