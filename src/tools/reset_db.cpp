#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <mysql/mysql.h>
#include "utils/config.h"
#include "utils/password.h"
#include "db_pool/connection_pool.h"

namespace fs = std::filesystem;

class DatabaseResetTool {
private:
    MYSQL* conn_;
    Config& config_;
    std::string test_case_dir_;

    bool executeSQL(const std::string& sql) {
        if (mysql_real_query(conn_, sql.c_str(), sql.size()) != 0) {
            std::cerr << "SQL error: " << mysql_error(conn_) << std::endl;
            std::cerr << "Query: " << sql << std::endl;
            return false;
        }
        return true;
    }

    std::string escapeString(const std::string& str) {
        std::string result;
        result.reserve(str.size() * 2 + 1);
        char* escaped = new char[str.size() * 2 + 1];
        mysql_real_escape_string(conn_, escaped, str.c_str(), str.size());
        result = escaped;
        delete[] escaped;
        return result;
    }

    void resetTables() {
        std::cout << "Resetting tables..." << std::endl;

        executeSQL("SET FOREIGN_KEY_CHECKS = 0");

        executeSQL("TRUNCATE TABLE submission_results");
        executeSQL("TRUNCATE TABLE judge_queue");
        executeSQL("TRUNCATE TABLE submissions");
        executeSQL("TRUNCATE TABLE test_cases");
        executeSQL("TRUNCATE TABLE problems");
        executeSQL("TRUNCATE TABLE users");

        executeSQL("SET FOREIGN_KEY_CHECKS = 1");

        std::cout << "All tables truncated." << std::endl;
    }

    bool createTestCaseDirectory() {
        try {
            fs::create_directories(test_case_dir_);
            return true;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Failed to create test case directory: " << e.what() << std::endl;
            return false;
        }
    }

    void createTestCaseFile(const std::string& problem_slug, int index,
                           const std::string& input, const std::string& output,
                           bool is_sample) {
        std::string input_path = test_case_dir_ + "/" + problem_slug + "_" + std::to_string(index) + ".in";
        std::string output_path = test_case_dir_ + "/" + problem_slug + "_" + std::to_string(index) + ".out";

        std::ofstream in_file(input_path);
        if (in_file.is_open()) {
            in_file << input;
            in_file.close();
        }

        std::ofstream out_file(output_path);
        if (out_file.is_open()) {
            out_file << output;
            out_file.close();
        }

        std::string input_escaped = escapeString(input_path);
        std::string output_escaped = escapeString(output_path);

        std::string sql = "INSERT INTO test_cases (problem_id, input_path, output_path, score, is_sample) "
                          "VALUES (1, '" + input_escaped + "', '" + output_escaped + "', 100, " +
                          (is_sample ? "1" : "0") + ")";
        executeSQL(sql);
    }

    void seedUsers() {
        std::cout << "Seeding users..." << std::endl;

        std::string admin_hash = PasswordUtil::hash("admin123");
        std::string testuser_hash = PasswordUtil::hash("Test123456");

        std::string sql = "INSERT INTO users (username, password_hash, role) VALUES "
                          "('admin', '" + escapeString(admin_hash) + "', 'admin'), "
                          "('testuser', '" + escapeString(testuser_hash) + "', 'user')";
        executeSQL(sql);

        std::cout << "Created admin user (admin/admin123)" << std::endl;
        std::cout << "Created test user (testuser/Test123456)" << std::endl;
    }

    void seedProblems() {
        std::cout << "Seeding problems..." << std::endl;

        std::string problems_sql = R"(
            INSERT INTO problems (title, description, difficulty, tags, time_limit_ms, memory_limit_mb, test_case_count) VALUES
            ('两数之和', '## 题目描述\n\n给定一个整数数组 nums 和一个整数目标值 target，请你在该数组中找出和为目标值 target 的那两个整数，并返回它们的数组下标。\n\n你可以假设每种输入只会对应一个答案，且同样的元素不能被重复利用。\n\n你可以在不使用额外空间的情况下解决此问题吗？\n\n## 示例\n\n**示例 1：**\n```\n输入：nums = [2,7,11,15], target = 9\n输出：[0,1]\n解释：因为 nums[0] + nums[1] == 9 ，返回 [0, 1] 。\n```\n\n**示例 2：**\n```\n输入：nums = [3,2,4], target = 6\n输出：[1,2]\n```\n\n**示例 3：**\n```\n输入：nums = [3,3], target = 6\n输出：[0,1]\n```\n\n## 提示\n\n- 2 <= nums.length <= 10^4\n- -10^9 <= nums[i] <= 10^9\n- -10^9 <= target <= 10^9\n- 只会存在一个有效答案', 'easy', '["算法", "数组", "哈希表"]', 1000, 256, 3),

            ('最长公共前缀', '## 题目描述\n\n编写一个函数来查找字符串数组中的最长公共前缀。\n\n如果不存在公共前缀，则返回空字符串 ""。\n\n## 示例\n\n**示例 1：**\n```\n输入：strs = ["flower","flow","flight"]\n输出："fl"\n```\n\n**示例 2：**\n```\n输入：strs = ["dog","racecar","car"]\n输出：""\n解释：输入不存在公共前缀。\n```\n\n## 提示\n\n- 1 <= strs.length <= 200\n- 0 <= strs[i].length <= 200\n- strs[i] 仅由小写英文字母组成', 'medium', '["算法", "字符串", "字典树"]', 1000, 256, 0),

            ('合并K个升序链表', '## 题目描述\n\n给定一个链表数组，每个链表都已经按升序排列。\n\n请将所有链表合并到一个升序链表中，返回合并后的链表。\n\n## 示例\n\n**示例 1：**\n```\n输入：lists = [[1,4,5],[1,3,4],[2,6]]\n输出：[1,1,2,3,4,4,5,6]\n解释：链表数组形式如上所述。\n```\n\n**示例 2：**\n```\n输入：lists = []\n输出：[]\n```\n\n**示例 3：**\n```\n输入：lists = [[]]\n输出：[]\n```\n\n## 提示\n\n- k == lists.length\n- 0 <= k <= 10^4\n- 0 <= lists[i].length <= 500\n- -10^4 <= lists[i][j] <= 10^4\n- lists[i] 按升序排列\n- lists[i].length 的总和不超过 10^4', 'hard', '["算法", "链表", "分治", "堆"]', 2000, 512, 0)
        )";

        executeSQL(problems_sql);
        std::cout << "Created 3 problems" << std::endl;
    }

    void seedTestCases() {
        std::cout << "Seeding test cases..." << std::endl;

        if (!createTestCaseDirectory()) {
            std::cerr << "Failed to create test case directory" << std::endl;
            return;
        }

        createTestCaseFile("two-sum", 1,
                           "4 9\n2 7 11 15\n",
                           "0 1\n",
                           true);

        createTestCaseFile("two-sum", 2,
                           "6 6\n3 2 4\n",
                           "1 2\n",
                           false);

        createTestCaseFile("two-sum", 3,
                           "2 6\n3 3\n",
                           "0 1\n",
                           false);

        executeSQL("UPDATE problems SET test_case_count = 3 WHERE id = 1");

        std::cout << "Created 3 test cases for problem 1" << std::endl;
    }

public:
    DatabaseResetTool() : config_(Config::instance()), conn_(nullptr) {
        test_case_dir_ = config_.app().test_case_dir;
    }

    bool run() {
        std::cout << "=== OJ System Database Reset Tool ===" << std::endl;
        std::cout << "Connecting to database..." << std::endl;

        ConnectionPool::instance().init(config_.database(), 5);
        conn_ = ConnectionPool::instance().getConnection();

        if (!conn_) {
            std::cerr << "Failed to get database connection" << std::endl;
            return false;
        }

        if (mysql_set_character_set(conn_, "utf8mb4") != 0) {
            std::cerr << "Failed to set charset: " << mysql_error(conn_) << std::endl;
            return false;
        }

        resetTables();
        seedUsers();
        seedProblems();
        seedTestCases();

        ConnectionPool::instance().returnConnection(conn_);
        ConnectionPool::instance().close();

        std::cout << "\n=== Database Reset Complete ===" << std::endl;
        std::cout << "Test data ready for automation testing:" << std::endl;
        std::cout << "  - Admin: admin / admin123" << std::endl;
        std::cout << "  - Test user: testuser / Test123456" << std::endl;
        std::cout << "  - Problem 1: 两数之和 (easy) with 3 test cases" << std::endl;
        std::cout << "  - Problem 2: 最长公共前缀 (medium)" << std::endl;
        std::cout << "  - Problem 3: 合并K个升序链表 (hard)" << std::endl;

        return true;
    }
};

int main(int argc, char* argv[]) {
    Config::instance().load("config/config.yaml");

    DatabaseResetTool tool;
    if (!tool.run()) {
        return 1;
    }
    return 0;
}
