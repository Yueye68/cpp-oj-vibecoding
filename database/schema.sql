-- ==================== 数据库初始化脚本 ====================
-- OJ 在线评测系统数据库 Schema

-- ==================== 用户表 ====================
-- 存储用户基本信息，区分普通用户和管理员
CREATE TABLE IF NOT EXISTS users (
    id INT PRIMARY KEY AUTO_INCREMENT,           -- 用户唯一ID，自增主键
    username VARCHAR(50) UNIQUE NOT NULL,        -- 用户名，唯一索引，长度50
    password_hash VARCHAR(255) NOT NULL,         -- bcrypt加密后的密码哈希
    role ENUM('user', 'admin') DEFAULT 'user',   -- 用户角色：user=普通用户，admin=管理员
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP  -- 注册时间
);

-- ==================== 题目表 ====================
-- 存储题目基本信息，包括描述、难度、限制等
CREATE TABLE IF NOT EXISTS problems (
    id INT PRIMARY KEY AUTO_INCREMENT,           -- 题目唯一ID，自增主键
    title VARCHAR(200) NOT NULL,                  -- 题目标题，长度200
    description TEXT,                             -- 题目描述，支持Markdown
    difficulty ENUM('easy', 'medium', 'hard') NOT NULL,  -- 难度等级：easy=简单，medium=中等，hard=困难
    tags JSON,                                     -- 分类标签，JSON数组格式，支持多选
    time_limit_ms INT DEFAULT 1000,               -- 时间限制，单位毫秒，默认1秒
    memory_limit_mb INT DEFAULT 256,               -- 内存限制，单位MB，默认256MB
    test_case_count INT DEFAULT 0,                -- 测试用例数量
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,     -- 创建时间
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP  -- 更新时间
);

-- ==================== 测试用例表 ====================
-- 存储每个题目的测试点，包括输入/输出文件路径
CREATE TABLE IF NOT EXISTS test_cases (
    id INT PRIMARY KEY AUTO_INCREMENT,           -- 测试用例唯一ID
    problem_id INT NOT NULL,                      -- 关联题目ID
    input_path VARCHAR(500) NOT NULL,             -- 输入文件存储路径
    output_path VARCHAR(500) NOT NULL,            -- 输出文件存储路径
    score INT DEFAULT 100,                         -- 该测试点分值，默认100
    is_sample TINYINT DEFAULT 0,                  -- 是否为示例测试点：0=否，1=是
    FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE  -- 删除题目时级联删除测试用例
);

-- ==================== 提交记录表 ====================
-- 存储用户的每次提交记录，包含代码和最终状态
CREATE TABLE IF NOT EXISTS submissions (
    id INT PRIMARY KEY AUTO_INCREMENT,           -- 提交记录唯一ID
    user_id INT NOT NULL,                         -- 提交用户ID
    problem_id INT NOT NULL,                      -- 对应题目ID
    code TEXT NOT NULL,                           -- 用户提交的源代码
    language VARCHAR(20) DEFAULT 'cpp',           -- 编程语言，默认C++
    status VARCHAR(20) NOT NULL,                  -- 最终状态：AC/WA/CE/RE/TLE/MLE/OLE/PE
    queue_status VARCHAR(20) DEFAULT 'pending',  -- 队列状态：pending/running/completed/failed
    error_detail TEXT,                            -- 编译/运行错误详情
    execute_time_ms INT,                          -- 总执行时间（毫秒）
    execute_memory_kb INT,                        -- 峰值内存占用（KB）
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,  -- 提交时间
    FOREIGN KEY (user_id) REFERENCES users(id),  -- 关联用户
    FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE  -- 关联题目
);

-- ==================== 提交详细结果表 ====================
-- 存储每个提交在各个测试点的详细结果
CREATE TABLE IF NOT EXISTS submission_results (
    id INT PRIMARY KEY AUTO_INCREMENT,           -- 结果记录唯一ID
    submission_id INT NOT NULL,                   -- 关联提交ID
    test_case_id INT NOT NULL,                    -- 关联测试用例ID
    status VARCHAR(20) NOT NULL,                  -- 该测试点状态：AC/WA/CE/RE/TLE/MLE/OLE/PE
    actual_output TEXT,                           -- 实际输出
    expected_output TEXT,                          -- 期望输出
    execute_time_ms INT,                          -- 该测试点执行时间（毫秒）
    execute_memory_kb INT,                        -- 该测试点内存占用（KB）
    FOREIGN KEY (submission_id) REFERENCES submissions(id) ON DELETE CASCADE,  -- 删除提交时级联删除结果
    FOREIGN KEY (test_case_id) REFERENCES test_cases(id)  -- 关联测试用例
);

-- ==================== 评测队列表 ====================
-- 存储待评测的提交任务，供 Worker 进程消费
CREATE TABLE IF NOT EXISTS judge_queue (
    id INT PRIMARY KEY AUTO_INCREMENT,           -- 队列记录唯一ID
    submission_id INT NOT NULL,                   -- 关联提交ID
    priority INT DEFAULT 0,                        -- 优先级，数值越小优先级越高
    status VARCHAR(20) DEFAULT 'pending',         -- 队列状态：pending/running/completed/failed
    worker_id VARCHAR(100),                       -- 处理此任务的 Worker ID
    retry_count INT DEFAULT 0,                    -- 重试次数
    error_message TEXT,                           -- 错误信息
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,  -- 入队时间
    started_at TIMESTAMP NULL,                    -- 开始处理时间
    completed_at TIMESTAMP NULL,                  -- 完成时间
    FOREIGN KEY (submission_id) REFERENCES submissions(id) ON DELETE CASCADE  -- 删除提交时级联删除队列记录
);
