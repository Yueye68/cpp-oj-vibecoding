#include "judge_worker.h"
#include "judge_queue.h"
#include "submission.h"
#include "problem.h"
#include "submission_result.h"
#include "judge_service.h"
#include "logger.h"
#include "config.h"
#include <chrono>
#include <sstream>

JudgeWorker::JudgeWorker(const std::string& workerId)
    : workerId_(workerId) {
}

JudgeWorker::~JudgeWorker() {
    stop();
}

void JudgeWorker::start() {
    if (running_.load()) {
        return;
    }
    running_ = true;
    stopRequested_ = false;
    workerThread_ = std::thread(&JudgeWorker::workerLoop, this);
    Logger::instance().info("Judge worker started: " + workerId_);
}

void JudgeWorker::stop() {
    if (!running_.load()) {
        return;
    }
    stopRequested_ = true;
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    Logger::instance().info("Judge worker stopped: " + workerId_);
}

void JudgeWorker::workerLoop() {
    while (!stopRequested_.load()) {
        if (!processNextJob()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(
                Config::instance().worker().poll_interval_ms));
        }
    }
}

bool JudgeWorker::processNextJob() {
    auto queueItemOpt = JudgeQueueItem::popPending(workerId_);
    if (!queueItemOpt.has_value()) {
        return false;
    }

    JudgeQueueItem queueItem = queueItemOpt.value();
    Logger::instance().info("Processing job: submission_id=" + std::to_string(queueItem.submission_id));

    auto submissionOpt = Submission::findById(queueItem.submission_id);
    if (!submissionOpt.has_value()) {
        Logger::instance().error("Submission not found: " + std::to_string(queueItem.submission_id));
        queueItem.markFailed("Submission not found");
        return true;
    }

    Submission submission = submissionOpt.value();

    auto problemOpt = Problem::findById(submission.problem_id);
    if (!problemOpt.has_value()) {
        Logger::instance().error("Problem not found: " + std::to_string(submission.problem_id));
        queueItem.markFailed("Problem not found");
        submission.queue_status = "failed";
        submission.status = "failed";
        submission.update();
        return true;
    }

    Problem problem = problemOpt.value();

    submission.queue_status = "running";
    submission.update();

    JudgeResult judgeResult = JudgeService::instance().judgeSubmission(submission, problem);

    submission.status = JudgeService::judgeStatusToString(judgeResult.overall_status);
    submission.error_detail = judgeResult.error_detail;
    submission.execute_time_ms = judgeResult.total_execution_time_ms;
    submission.execute_memory_kb = judgeResult.peak_memory_kb;
    submission.queue_status = "completed";
    submission.update();

    for (const auto& tcResult : judgeResult.test_case_results) {
        SubmissionResult sr;
        sr.submission_id = submission.id;
        sr.test_case_id = tcResult.test_case_id;
        sr.status = JudgeService::judgeStatusToString(tcResult.status);
        sr.actual_output = tcResult.actual_output;
        sr.expected_output = tcResult.expected_output;
        sr.execute_time_ms = tcResult.execution_time_ms;
        sr.execute_memory_kb = tcResult.memory_kb;
        sr.create();
    }

    queueItem.markCompleted();

    Logger::instance().info("Job completed: submission_id=" + std::to_string(submission.id) +
                           ", status=" + submission.status);

    return true;
}