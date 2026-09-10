# C++ OJ 在线评测系统 — 简历描述

**项目名称**：C++ OJ System（仿 LeetCode 在线评测平台）

**技术栈**：C++20 · cpp-httplib · MySQL · Linux cgroup v1 · bcrypt/OpenSSL · GoogleTest · CodeMirror 6 · 原生 HTML/CSS/JS

**简介**：全栈 C++ 实现的在线评测系统，采用 HTTP 主服务 + 评测 Worker 双进程架构，基于数据库任务队列实现异步评测，评测过程通过 Linux cgroup v1 沙箱隔离 CPU/内存/PID 资源。

## 核心功能

- **HTTP 服务层**：基于 cpp-httplib 实现 Router / Handler / 中间件分层，注册 17+ REST API，自研中间件完成登录态与角色校验。

- **数据层**：自研单例 `ConnectionPool`（mutex + condition_variable，FIFO），封装 6 张表 ORM 模型（User / Problem / TestCase / Submission / SubmissionResult / JudgeQueue），支持完整 CRUD。

- **安全与会话**：基于 OpenSSL 随机生成 Session token，服务端存储 + HttpOnly Cookie；密码 bcrypt 哈希存储。

- **沙箱评测**：基于 Linux cgroup v1 限制 CPU / 内存 / 进程数，配合 `waitpid + SIGKILL` 双层超时保护，支持 AC / WA / CE / RE / TLE / MLE / OLE / PE 八种评测状态及每测试点耗时与内存统计。

- **异步评测流水线**：提交 → 入库 `judge_queue` → Worker 拉取 → g++ -O2 编译 → 测试点循环执行 → 结果入库，请求不阻塞。

- **管理后台**：原生前端实现题目 / 测试用例增删改查、Markdown 描述编辑、批量上传测试点。

## 测试体系

- 基于 **GoogleTest** 构建单元测试 + 集成测试，覆盖工具层（配置 / 日志 / 密码 / Session）、基础设施层（连接池 / ORM）、服务层（各 Handler）、沙箱层（cgroup / 评测服务）。

- 通过 **CMake + ctest** 一键运行全部用例，失败用例支持详细输出。

- 编写规格说明书、API 文档、部署文档等完整项目文档。