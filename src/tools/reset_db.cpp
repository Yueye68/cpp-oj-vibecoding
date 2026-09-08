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

        std::string problems_sql = R"oj(
            INSERT INTO problems (title, description, difficulty, tags, time_limit_ms, memory_limit_mb, test_case_count) VALUES
            ('两数之和', '## 题目描述\n\n给定一个整数数组 nums 和一个整数目标值 target，请你在该数组中找出和为目标值 target 的那两个整数，并返回它们的数组下标。\n\n你可以假设每种输入只会对应一个答案，且同样的元素不能被重复利用。\n\n你可以在不使用额外空间的情况下解决此问题吗？\n\n## 示例\n\n**示例 1：**\n```\n输入：nums = [2,7,11,15], target = 9\n输出：[0,1]\n解释：因为 nums[0] + nums[1] == 9 ，返回 [0, 1] 。\n```\n\n**示例 2：**\n```\n输入：nums = [3,2,4], target = 6\n输出：[1,2]\n```\n\n**示例 3：**\n```\n输入：nums = [3,3], target = 6\n输出：[0,1]\n```\n\n## 提示\n\n- 2 <= nums.length <= 10^4\n- -10^9 <= nums[i] <= 10^9\n- -10^9 <= target <= 10^9\n- 只会存在一个有效答案', 'easy', '["算法", "数组", "哈希表"]', 1000, 256, 3),

            ('最长公共前缀', '## 题目描述\n\n编写一个函数来查找字符串数组中的最长公共前缀。\n\n如果不存在公共前缀，则返回空字符串 ""。\n\n## 示例\n\n**示例 1：**\n```\n输入：strs = ["flower","flow","flight"]\n输出："fl"\n```\n\n**示例 2：**\n```\n输入：strs = ["dog","racecar","car"]\n输出：""\n解释：输入不存在公共前缀。\n```\n\n## 提示\n\n- 1 <= strs.length <= 200\n- 0 <= strs[i].length <= 200\n- strs[i] 仅由小写英文字母组成', 'medium', '["算法", "字符串", "字典树"]', 1000, 256, 0),

            ('合并K个升序链表', '## 题目描述\n\n给定一个链表数组，每个链表都已经按升序排列。\n\n请将所有链表合并到一个升序链表中，返回合并后的链表。\n\n## 示例\n\n**示例 1：**\n```\n输入：lists = [[1,4,5],[1,3,4],[2,6]]\n输出：[1,1,2,3,4,4,5,6]\n解释：链表数组形式如上所述。\n```\n\n**示例 2：**\n```\n输入：lists = []\n输出：[]\n```\n\n**示例 3：**\n```\n输入：lists = [[]]\n输出：[]\n```\n\n## 提示\n\n- k == lists.length\n- 0 <= k <= 10^4\n- 0 <= lists[i].length <= 500\n- -10^4 <= lists[i][j] <= 10^4\n- lists[i] 按升序排列\n- lists[i].length 的总和不超过 10^4', 'hard', '["算法", "链表", "分治", "堆"]', 2000, 512, 0),

            ('翻转链表', '## 题目描述\n\n给定单链表的头节点 head ，请反转链表，并返回反转后的链表。\n\n## 示例\n\n**示例 1：**\n```\n输入：head = [1,2,3,4,5]\n输出：[5,4,3,2,1]\n```\n\n**示例 2：**\n```\n输入：head = [1,2]\n输出：[2,1]\n```\n\n**示例 3：**\n```\n输入：head = []\n输出：[]\n```\n\n## 提示\n\n- 链表中节点的数目范围是 [0, 5000]', 'easy', '["算法", "链表"]', 1000, 256, 0),

            ('有效的括号', '## 题目描述\n\n给定一个只包括 ''(''，'')''，''{''，''}''，''[''，'']'' 的字符串 s ，判断字符串是否有效。\n\n有效字符串需满足：\n1. 左括号必须用相同类型的右括号闭合。\n2. 左括号必须以正确的顺序闭合。\n3. 每个右括号都有一个对应的相同类型的左括号。\n\n## 示例\n\n**示例 1：**\n```\n输入：s = "()"\n输出：true\n```\n\n**示例 2：**\n```\n输入：s = "()[]{}"\n输出：true\n```\n\n**示例 3：**\n```\n输入：s = "(]"\n输出：false\n```\n\n**示例 4：**\n```\n输入：s = "([)]"\n输出：false\n```\n\n## 提示\n\n- 1 <= s.length <= 10^4\n- s 仅由括号 ''()[]{}'' 组成', 'easy', '["算法", "栈", "字符串"]', 1000, 256, 0),

            ('买卖股票的最佳时机', '## 题目描述\n\n给定一个数组 prices ，它的第 i 个元素 prices[i] 表示一支给定股票第 i 天的价格。\n\n你只能选择 某一天 买入这只股票，并选择在 未来的某一个不同的日子 卖出该股票。设计一个算法来计算你所能获取的最大利润。\n\n返回你可以从这笔交易中获取的最大利润。如果你不能获取任何利润，返回 0 。\n\n## 示例\n\n**示例 1：**\n```\n输入：[7,1,5,3,6,4]\n输出：5\n解释：在第 2 天（股票价格 = 1）的时候买入，在第 5 天（股票价格 = 6）的时候卖出，最大利润 = 6-1 = 5 。\n```\n\n**示例 2：**\n```\n输入：prices = [7,6,4,3,1]\n输出：0\n解释：在这种情况下，没有交易完成，所以最大利润为 0。\n```\n\n## 提示\n\n- 1 <= prices.length <= 10^5\n- 0 <= prices[i] <= 10^4', 'easy', '["算法", "数组", "动态规划"]', 1000, 256, 0),

            ('二分查找', '## 题目描述\n\n给定一个 n 个元素有序的（升序）整型数组 nums 和一个目标值 target ，写一个函数搜索 nums 中的 target，如果目标值存在返回下标，否则返回 -1。\n\n你必须设计一个时间复杂度为 O(log n) 的算法解决此问题。\n\n## 示例\n\n**示例 1：**\n```\n输入: nums = [-1,0,3,5,9,12], target = 9\n输出: 4\n解释: 9 出现在 nums 中并且下标为 4\n```\n\n**示例 2：**\n```\n输入: nums = [-1,0,3,5,9,12], target = 2\n输出: -1\n解释: 2 不存在 nums 中因此返回 -1\n```\n\n## 提示\n\n- 你可以假设 nums 中的所有元素是不重复的。\n- n 将在 [1, 10000] 之间。\n- nums 中的每个元素都将在 [-9999, 9999] 之间。', 'easy', '["算法", "数组", "二分查找"]', 1000, 256, 0),

            ('最大子序和', '## 题目描述\n\n给你一个整数数组 nums ，请你找出一个具有最大和的连续子数组（子数组最少包含一个元素），返回其最大和。\n\n## 示例\n\n**示例 1：**\n```\n输入：nums = [-2,1,-3,4,-1,2,1,-5,4]\n输出：6\n解释：连续子数组 [4,-1,2,1] 的和最大，为 6 。\n```\n\n**示例 2：**\n```\n输入：nums = [1]\n输出：1\n```\n\n**示例 3：**\n```\n输入：nums = [5,4,-1,7,8]\n输出：23\n```\n\n## 提示\n\n- 1 <= nums.length <= 10^5\n- -10^4 <= nums[i] <= 10^4', 'easy', '["算法", "数组", "动态规划", "分治"]', 1000, 256, 0),

            ('爬楼梯', '## 题目描述\n\n假设你正在爬楼梯。需要 n 阶你到达顶部。\n\n每次你可以爬 1 或 2 个台阶。你有多少种不同的方法可以爬到顶部？\n\n## 示例\n\n**示例 1：**\n```\n输入：n = 2\n输出：2\n解释：有两种方法可以爬到顶部。\n1. 1 阶 + 1 阶\n2. 2 阶\n```\n\n**示例 2：**\n```\n输入：n = 3\n输出：3\n解释：有三种方法可以爬到顶部。\n1. 1 阶 + 1 阶 + 1 阶\n2. 1 阶 + 2 阶\n3. 2 阶 + 1 阶\n```\n\n## 提示\n\n- 1 <= n <= 45', 'easy', '["算法", "动态规划", "数学"]', 1000, 256, 0),

            ('合并两个有序数组', '## 题目描述\n\n给你两个按 非递减顺序 排列的整数数组 nums1 和 nums2，另有两个整数 m 和 n ，分别表示 nums1 和 nums2 中的元素数目。\n\n请你 合并 nums2 到 nums1 中，使合并后的数组同样按 非递减顺序 排列。\n\n注意：最终，合并后数组不应由函数返回，而是存储在数组 nums1 中。为了应对这种情况，nums1 的初始长度为 m + n，其中前 m 个元素表示应合并的元素，后 n 个元素为 0 ，应忽略。nums2 的长度为 n 。\n\n## 示例\n\n**示例 1：**\n```\n输入：nums1 = [1,2,3,0,0,0], m = 3, nums2 = [2,5,6], n = 3\n输出：[1,2,2,3,5,6]\n解释：需要合并 [1,2,3] 和 [2,5,6] 。\n合并结果是 [1,2,2,3,5,6] 。\n```\n\n## 提示\n\n- nums1.length == m + n\n- nums2.length == n\n- 0 <= m, n <= 200', 'easy', '["算法", "数组", "双指针", "排序"]', 1000, 256, 0),

            ('删除排序数组中的重复项', '## 题目描述\n\n给你一个 升序排列 的数组 nums ，请你 原地 删除重复出现的元素，使每个元素 只出现一次 ，返回删除后数组的新长度。元素的 相对顺序 应该保持 一致 。\n\n由于在某些语言中不能改变数组的长度，所以必须将结果放在数组 nums 的第一部分。更规范地说，如果在删除重复项之后有 k 个元素，那么 nums 的前 k 个元素应该保存最终结果。\n\n## 示例\n\n**示例 1：**\n```\n输入：nums = [1,1,2]\n输出：2, nums = [1,2,_]\n解释：函数应该返回新的长度 2 ，并且原数组 nums 的前两个元素被修改为 1, 2 。不需要考虑数组中超出新长度后面的元素。\n```\n\n**示例 2：**\n```\n输入：nums = [0,0,1,1,1,2,2,3,3,4]\n输出：5, nums = [0,1,2,3,4]\n解释：函数应该返回新的长度 5 ，并且原数组 nums 的前五个元素被修改为 0, 1, 2, 3, 4 。不需要考虑数组中超出新长度后面的元素。\n```\n\n## 提示\n\n- 1 <= nums.length <= 3 * 10^4', 'easy', '["算法", "数组", "双指针"]', 1000, 256, 0),

            ('二叉树的最大深度', '## 题目描述\n\n给定一个二叉树 root ，返回其最大深度。\n\n二叉树的 最大深度 是指从根节点到最远叶子节点的最长路径上的节点数。\n\n## 示例\n\n**示例 1：**\n```\n输入：root = [3,9,20,null,null,15,7]\n输出：3\n```\n\n**示例 2：**\n```\n输入：root = [1,null,2]\n输出：2\n```\n\n**示例 3：**\n```\n输入：root = []\n输出：0\n```\n\n## 提示\n\n- 树中节点的数量在 [0, 10^4] 区间内。', 'easy', '["算法", "树", "深度优先搜索", "广度优先搜索"]', 1000, 256, 0),

            ('字符串中的第一个唯一字符', '## 题目描述\n\n给定一个字符串 s ，找到 它的第一个不重复出现的字符，并返回它的索引 。如果不存在，则返回 -1 。\n\n## 示例\n\n**示例 1：**\n```\n输入: s = "leetcode"\n输出: 0\n```\n\n**示例 2：**\n```\n输入: s = "loveleetcode"\n输出: 2\n```\n\n**示例 3：**\n```\n输入: s = "aabb"\n输出: -1\n```\n\n## 提示\n\n- 1 <= s.length <= 10^5\n- s 只包含小写字母', 'easy', '["算法", "哈希表", "字符串"]', 1000, 256, 0),

            ('回文链表', '## 题目描述\n\n给你一个单链表的头节点 head ，请你判断该链表是否为回文链表。如果是，返回 true ；否则，返回 false 。\n\n## 示例\n\n**示例 1：**\n```\n输入：head = [1,2,2,1]\n输出：true\n```\n\n**示例 2：**\n```\n输入：head = [1,2]\n输出：false\n```\n\n## 提示\n\n- 链表中节点数目在范围 [1, 10^5] 内', 'easy', '["算法", "链表", "双指针", "栈"]', 1000, 256, 0),

            ('移动零', '## 题目描述\n\n给定一个数组 nums，编写一个函数将所有 0 移动到数组的末尾，同时保持非零元素的相对顺序。\n\n请注意 ，必须在不复制数组的情况下原地对数组进行操作。\n\n## 示例\n\n**示例 1：**\n```\n输入: nums = [0,1,0,3,12]\n输出: [1,3,12,0,0]\n```\n\n**示例 2：**\n```\n输入: nums = [0]\n输出: [0]\n```\n\n## 提示\n\n- 1 <= nums.length <= 10^4', 'easy', '["算法", "数组", "双指针"]', 1000, 256, 0),

            ('无重复字符的最长子串', '## 题目描述\n\n给定一个字符串 s ，请你找出其中不含有重复字符的 最长子串 的长度。\n\n## 示例\n\n**示例 1：**\n```\n输入: s = "abcabcbb"\n输出: 3\n解释: 因为无重复字符的最长子串是 "abc"，所以其长度为 3。\n```\n\n**示例 2：**\n```\n输入: s = "bbbbb"\n输出: 1\n解释: 因为无重复字符的最长子串是 "b"，所以其长度为 1。\n```\n\n**示例 3：**\n```\n输入: s = "pwwkew"\n输出: 3\n解释: 因为无重复字符的最长子串是 "wke"，所以其长度为 3。\n```\n\n## 提示\n\n- 0 <= s.length <= 5 * 10^4', 'medium', '["算法", "哈希表", "字符串", "滑动窗口"]', 1000, 256, 0),

            ('最小栈', '## 题目描述\n\n设计一个支持 push ，pop ，top 操作，并能在常数时间内检索到最小元素的栈。\n\n实现 MinStack 类:\n- MinStack() 初始化堆栈对象。\n- void push(int val) 将元素 val 推入堆栈。\n- void pop() 删除堆栈顶部的元素。\n- int top() 获取堆栈顶部的元素。\n- int getMin() 获取堆栈中的最小元素。\n\n## 示例\n\n**示例 1：**\n```\n输入：\n["MinStack","push","push","push","getMin","pop","top","getMin"]\n[[],[-2],[0],[-3],[],[],[],[]]\n\n输出：\n[null,null,null,null,-3,null,0,-2]\n\n解释：\nMinStack minStack = new MinStack();\nminStack.push(-2);\nminStack.push(0);\nminStack.push(-3);\nminStack.getMin();   --> 返回 -3.\nminStack.pop();\nminStack.top();      --> 返回 0.\nminStack.getMin();   --> 返回 -2.\n```\n\n## 提示\n\n- -2^31 <= val <= 2^31 - 1\n- pop、top 和 getMin 操作总是在 非空栈 上调用', 'medium', '["算法", "栈", "设计"]', 1000, 256, 0),

            ('排序链表', '## 题目描述\n\n给你链表的头结点 head ，请将其按 升序 排列并返回 排序后的链表 。\n\n## 示例\n\n**示例 1：**\n```\n输入：head = [4,2,1,3]\n输出：[1,2,3,4]\n```\n\n**示例 2：**\n```\n输入：head = [-1,5,3,4,0]\n输出：[-1,0,3,4,5]\n```\n\n**示例 3：**\n```\n输入：head = []\n输出：[]\n```\n\n## 提示\n\n- 链表中节点的数目在范围 [0, 5 * 10^4] 内', 'medium', '["算法", "链表", "双指针", "分治", "排序"]', 2000, 256, 0),

            ('搜索旋转排序数组', '## 题目描述\n\n整数数组 nums 按升序排列，数组中的值 互不相同 。\n\n在传递给函数之前，nums 在预先未知的某个下标 k 上进行了 旋转，使数组变为 [nums[k], nums[k+1], ..., nums[n-1], nums[0], nums[1], ..., nums[k-1]]。\n\n给你 旋转后 的数组 nums 和一个整数 target ，如果 nums 中存在这个目标值 target ，则返回它的下标，否则返回 -1 。\n\n你必须设计一个时间复杂度为 O(log n) 的算法解决此问题。\n\n## 示例\n\n**示例 1：**\n```\n输入：nums = [4,5,6,7,0,1,2], target = 0\n输出：4\n```\n\n**示例 2：**\n```\n输入：nums = [4,5,6,7,0,1,2], target = 3\n输出：-1\n```\n\n**示例 3：**\n```\n输入：nums = [1], target = 0\n输出：-1\n```\n\n## 提示\n\n- 1 <= nums.length <= 5000', 'medium', '["算法", "数组", "二分查找"]', 1000, 256, 0),

            ('单词搜索', '## 题目描述\n\n给定一个 m x n 二维字符网格 board 和一个字符串单词 word 。如果 word 存在于网格中，返回 true ；否则，返回 false 。\n\n单词必须按照字母顺序，通过相邻的单元格内的字母构成，其中"相邻"单元格是那些水平相邻或垂直相邻的单元格。同一个单元格内的字母不允许被重复使用。\n\n## 示例\n\n**示例 1：**\n```\n输入：board = [["A","B","C","E"],["S","F","C","S"],["A","D","E","E"]], word = "ABCCED"\n输出：true\n```\n\n**示例 2：**\n```\n输入：board = [["A","B","C","E"],["S","F","C","S"],["A","D","E","E"]], word = "SEE"\n输出：true\n```\n\n**示例 3：**\n```\n输入：board = [["A","B","C","E"],["S","F","C","S"],["A","D","E","E"]], word = "ABCB"\n输出：false\n```\n\n## 提示\n\n- m == board.length\n- n = board[i].length\n- 1 <= m, n <= 6', 'medium', '["算法", "数组", "回溯", "矩阵"]', 2000, 256, 0),

            ('岛屿数量', '## 题目描述\n\n给你一个由 ''1''（陆地）和 ''0''（水）组成的的二维网格，请你计算网格中岛屿的数量。\n\n岛屿总是被水包围，并且每座岛屿只能由水平方向和/或竖直方向上相邻的陆地连接形成。\n\n此外，你可以假设该网格的四条边均被水包围。\n\n## 示例\n\n**示例 1：**\n```\n输入：grid = [\n  ["1","1","1","1","0"],\n  ["1","1","0","1","0"],\n  ["1","1","0","0","0"],\n  ["0","0","0","0","0"]\n]\n输出：1\n```\n\n**示例 2：**\n```\n输入：grid = [\n  ["1","1","0","0","0"],\n  ["1","1","0","0","0"],\n  ["0","0","1","0","0"],\n  ["0","0","0","1","1"]\n]\n输出：3\n```\n\n## 提示\n\n- m == grid.length\n- n = grid[i].length\n- 1 <= m, n <= 300', 'medium', '["算法", "深度优先搜索", "广度优先搜索", "并查集", "数组", "矩阵"]', 2000, 256, 0),

            ('课程表', '## 题目描述\n\n你这个学期必须选修 numCourses 门课程，记为 0 到 numCourses - 1 。\n\n在选修某些课程之前需要一些先修课程。先修课程按数组 prerequisites 给出，其中 prerequisites[i] = [ai, bi] ，表示如果要学习课程 ai 则 必须 先学习课程 bi 。\n\n例如，先修课程对 [0, 1] 表示：想要学习课程 0 ，你需要先完成课程 1 。\n\n请你判断是否可能完成所有课程的学习？如果可以，返回 true ；否则，返回 false 。\n\n## 示例\n\n**示例 1：**\n```\n输入：numCourses = 2, prerequisites = [[1,0]]\n输出：true\n解释：总共有 2 门课程。学习课程 1 之前，你需要完成课程 0 。这是可能的。\n```\n\n**示例 2：**\n```\n输入：numCourses = 2, prerequisites = [[1,0],[0,1]]\n输出：false\n解释：总共有 2 门课程。学习课程 1 之前，你需要先完成课程 0 ，并且学习课程 0 之前，你还应先完成课程 1 。这是不可能的。\n```\n\n## 提示\n\n- 1 <= numCourses <= 2000', 'medium', '["算法", "深度优先搜索", "广度优先搜索", "图", "拓扑排序"]', 2000, 256, 0),

            ('实现 Trie（前缀树）', '## 题目描述\n\nTrie（发音类似 "try"）或者说 前缀树 是一种树形数据结构，用于高效地存储和检索字符串数据集中的键。这一数据结构有相当多的应用情景，例如自动补完和拼写检查。\n\n请你实现 Trie 类：\n- Trie() 初始化前缀树对象。\n- void insert(String word) 向前缀树中插入字符串 word 。\n- boolean search(String word) 如果字符串 word 在前缀树中，返回 true（即，在检索之前已经插入）；否则，返回 false 。\n- boolean startsWith(String prefix) 如果之前已经插入的字符串 word 的前缀之一为 prefix ，返回 true ；否则，返回 false 。\n\n## 示例\n\n**示例 1：**\n```\n输入：\n["Trie", "insert", "search", "search", "startsWith", "startsWith", "insert", "search"]\n[[], ["apple"], ["apple"], ["app"], ["app"], ["app"], ["app"], ["app"]]\n输出：\n[null, null, true, false, true, true, null, true]\n\n解释：\nTrie trie = new Trie();\ntrie.insert("apple");\ntrie.search("apple");   // 返回 True\ntrie.search("app");     // 返回 False\ntrie.startsWith("app"); // 返回 True\ntrie.insert("app");\ntrie.search("app");     // 返回 True\n```\n\n## 提示\n\n- 1 <= word.length, prefix.length <= 2000\n- word 和 prefix 仅由小写英文字母组成', 'medium', '["算法", "字典树", "设计", "字符串"]', 1000, 256, 0),

            ('接雨水', '## 题目描述\n\n给定 n 个非负整数表示每个宽度为 1 的柱子的高度图，计算按此排列的柱子，下雨之后能接多少雨水。\n\n## 示例\n\n**示例 1：**\n```\n输入：height = [0,1,0,2,1,0,1,3,2,1,2,1]\n输出：6\n解释：上面是由数组 [0,1,0,2,1,0,1,3,2,1,2,1] 表示的高度图，在这种情况下，可以接 6 个单位的雨水（蓝色部分表示雨水）。\n```\n\n**示例 2：**\n```\n输入：height = [4,2,0,3,2,5]\n输出：9\n```\n\n## 提示\n\n- n == height.length\n- 1 <= n <= 2 * 10^4', 'hard', '["算法", "数组", "双指针", "动态规划", "栈", "单调栈"]', 2000, 512, 0),

            ('最小路径和', '## 题目描述\n\n给定一个包含非负整数的 m x n 网格 grid ，请找出一条从左上角到右下角的路径，使得路径上的数字总和为最小。\n\n说明：每次只能向下或者向右移动一步。\n\n## 示例\n\n**示例 1：**\n```\n输入：grid = [[1,3,1],[1,5,1],[4,2,1]]\n输出：7\n解释：因为路径 1→3→1→1→1 的总和最小。\n```\n\n**示例 2：**\n```\n输入：grid = [[1,2,3],[4,5,6]]\n输出：12\n```\n\n## 提示\n\n- m == grid.length\n- n = grid[i].length\n- 1 <= m, n <= 200', 'medium', '["算法", "数组", "动态规划", "矩阵"]', 1000, 256, 0),

            ('多数元素', '## 题目描述\n\n给定一个大小为 n 的数组 nums ，返回其中的多数元素。多数元素是指在数组中出现次数 大于 ⌊ n/2 ⌋ 的元素。\n\n你可以假设数组是非空的，并且给定的数组总是存在多数元素。\n\n## 示例\n\n**示例 1：**\n```\n输入：nums = [3,2,3]\n输出：3\n```\n\n**示例 2：**\n```\n输入：nums = [2,2,1,1,1,2,2]\n输出：2\n```\n\n## 提示\n\n- n == nums.length\n- 1 <= n <= 5 * 10^4', 'easy', '["算法", "数组", "哈希表", "分治", "计数", "排序"]', 1000, 256, 0)
        )oj";

        executeSQL(problems_sql);
        std::cout << "Created 26 problems (pageSize=20 -> 2 pages for pagination testing)" << std::endl;
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
        std::cout << "  - 26 problems (pageSize=20 -> 2 pages, supports pagination testing)" << std::endl;
        std::cout << "  - Problem 1: 两数之和 (easy) with 3 test cases" << std::endl;
        std::cout << "  - Problems 2-26: 题目列表分页可测试" << std::endl;

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
