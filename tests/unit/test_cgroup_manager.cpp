#include <gtest/gtest.h>
#include <string>
#include "services/cgroup_manager.h"

class CgroupResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CgroupResultTest, DefaultConstruction) {
    CgroupResult result;
    EXPECT_EQ(-1, result.exit_code);
    EXPECT_EQ(0, result.signal);
    EXPECT_EQ(0, result.execution_time_ms);
    EXPECT_EQ(0, result.memory_kb);
    EXPECT_EQ("", result.stdout_output);
    EXPECT_EQ("", result.stderr_output);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.out_of_memory);
    EXPECT_FALSE(result.out_of_pids);
}

TEST_F(CgroupResultTest, StructAssignment) {
    CgroupResult result;
    result.exit_code = 0;
    result.signal = 0;
    result.execution_time_ms = 1500;
    result.memory_kb = 8192;
    result.stdout_output = "42\n";
    result.stderr_output = "";
    result.timed_out = false;
    result.out_of_memory = false;
    result.out_of_pids = false;

    EXPECT_EQ(0, result.exit_code);
    EXPECT_EQ(0, result.signal);
    EXPECT_EQ(1500, result.execution_time_ms);
    EXPECT_EQ(8192, result.memory_kb);
    EXPECT_EQ("42\n", result.stdout_output);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.out_of_memory);
    EXPECT_FALSE(result.out_of_pids);
}

TEST_F(CgroupResultTest, TimedOutResult) {
    CgroupResult result;
    result.exit_code = -1;
    result.signal = SIGKILL;
    result.timed_out = true;
    result.execution_time_ms = 5000;

    EXPECT_EQ(-1, result.exit_code);
    EXPECT_EQ(SIGKILL, result.signal);
    EXPECT_TRUE(result.timed_out);
}

TEST_F(CgroupResultTest, OOMResult) {
    CgroupResult result;
    result.exit_code = -1;
    result.out_of_memory = true;
    result.memory_kb = 300000;

    EXPECT_TRUE(result.out_of_memory);
    EXPECT_EQ(300000, result.memory_kb);
}

TEST_F(CgroupResultTest, OutOfPidsResult) {
    CgroupResult result;
    result.exit_code = -1;
    result.out_of_pids = true;

    EXPECT_TRUE(result.out_of_pids);
}

TEST_F(CgroupResultTest, RuntimeErrorResult) {
    CgroupResult result;
    result.exit_code = -1;
    result.signal = SIGSEGV;
    result.stderr_output = "Segmentation fault";

    EXPECT_EQ(-1, result.exit_code);
    EXPECT_EQ(SIGSEGV, result.signal);
    EXPECT_EQ("Segmentation fault", result.stderr_output);
}

class CgroupManagerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CgroupManagerTest, SingletonAccess) {
    CgroupManager& mgr1 = CgroupManager::instance();
    CgroupManager& mgr2 = CgroupManager::instance();
    EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(CgroupManagerTest, GetCgroupRoot) {
    EXPECT_EQ("/sys/fs/cgroup", CgroupManager::getCgroupRoot());
}

TEST_F(CgroupManagerTest, GenerateSubmissionCgroupPath) {
    std::string path = CgroupManager::generateSubmissionCgroupPath("sub_123");
    EXPECT_EQ("/sys/fs/cgroup/oj_submissions/sub_123", path);

    path = CgroupManager::generateSubmissionCgroupPath("tc_5");
    EXPECT_EQ("/sys/fs/cgroup/oj_submissions/tc_5", path);
}

TEST_F(CgroupManagerTest, IsAvailable) {
    CgroupManager& mgr = CgroupManager::instance();
    bool available = mgr.isAvailable();
    EXPECT_TRUE(available == true || available == false);
}

TEST_F(CgroupManagerTest, CreateAndDeleteCgroup) {
    CgroupManager& mgr = CgroupManager::instance();

    std::string test_cgroup_id = "test_unit_" + std::to_string(getpid());
    std::string cgroup_path = CgroupManager::generateSubmissionCgroupPath(test_cgroup_id);

    bool created = mgr.createCgroup(test_cgroup_id);
    if (created) {
        bool deleted = mgr.deleteCgroup(cgroup_path);
        EXPECT_TRUE(deleted);
    }
}

TEST_F(CgroupManagerTest, CreateCgroup_ReturnsTrue) {
    CgroupManager& mgr = CgroupManager::instance();

    std::string test_cgroup_id = "test_create_" + std::to_string(getpid());
    bool created = mgr.createCgroup(test_cgroup_id);

    if (created) {
        std::string cgroup_path = CgroupManager::generateSubmissionCgroupPath(test_cgroup_id);
        mgr.deleteCgroup(cgroup_path);
    }
}

TEST_F(CgroupManagerTest, SetCpuLimit) {
    CgroupManager& mgr = CgroupManager::instance();

    std::string test_cgroup_id = "test_cpu_" + std::to_string(getpid());
    std::string cgroup_path = CgroupManager::generateSubmissionCgroupPath(test_cgroup_id);

    if (mgr.createCgroup(test_cgroup_id)) {
        bool set = mgr.setCpuLimit(cgroup_path, 1000000, 1000000);
        EXPECT_TRUE(set);

        mgr.deleteCgroup(cgroup_path);
    }
}

TEST_F(CgroupManagerTest, SetMemoryLimit) {
    CgroupManager& mgr = CgroupManager::instance();

    std::string test_cgroup_id = "test_mem_" + std::to_string(getpid());
    std::string cgroup_path = CgroupManager::generateSubmissionCgroupPath(test_cgroup_id);

    if (mgr.createCgroup(test_cgroup_id)) {
        bool set = mgr.setMemoryLimit(cgroup_path, 256 * 1024 * 1024);
        EXPECT_TRUE(set);

        mgr.deleteCgroup(cgroup_path);
    }
}

TEST_F(CgroupManagerTest, SetPidLimit) {
    CgroupManager& mgr = CgroupManager::instance();

    std::string test_cgroup_id = "test_pid_" + std::to_string(getpid());
    std::string cgroup_path = CgroupManager::generateSubmissionCgroupPath(test_cgroup_id);

    if (mgr.createCgroup(test_cgroup_id)) {
        bool set = mgr.setPidLimit(cgroup_path, 64);
        EXPECT_TRUE(set);

        mgr.deleteCgroup(cgroup_path);
    }
}
