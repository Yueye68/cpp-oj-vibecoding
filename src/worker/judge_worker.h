#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <vector>

class JudgeWorker {
public:
    JudgeWorker(const std::string& workerId);
    ~JudgeWorker();

    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

private:
    void workerLoop();
    bool processNextJob();

    std::string workerId_;
    std::thread workerThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
};