# C++ OJ System

一个面向教学场景、轻量级的在线评测（Online Judge）系统，仿 LeetCode 的核心交互体验，使用 **C++ + cpp-httplib + MySQL** 前后端一体化实现，代码评测在 **cgroup** 隔离环境中完成。

> 项目代号：**cpp-oj-vibecoding**
> 目标用户：教师（出题/管理）、学生（刷题/练习）

---

## 目录

- [项目特色](#项目特色)
- [功能一览](#功能一览)
- [技术栈](#技术栈)
- [快速开始](#快速开始)
- [目录结构](#目录结构)
- [核心模块](#核心模块)
- [API 概览](#api-概览)
- [数据库 Schema](#数据库-schema)
- [评测状态码](#评测状态码)
- [安全与隔离](#安全与隔离)
- [测试](#测试)
- [文档索引](#文档索引)
- [路线图](#路线图)
- [许可](#许可)

---

## 项目特色

- **全栈 C++**：HTTP 服务、数据库 ORM、评测 worker 全部用现代 C++ (C++20) 编写，无第三方重量级框架。
- **单文件 HTTP 服务**：[cpp-httplib](https://github.com/yhirose/cpp-httplib) 单头文件即可构建一个高性能 HTTP 服务器。
- **真实沙箱隔离**：使用 Linux cgroup 对评测进程进行 CPU、内存、PID、网络等资源限制，避免恶意代码影响宿主机。
- **异步评测队列**：用户提交 → 入库 → 评测 worker 拉取 → 异步执行，不阻塞 HTTP 响应。
- **零前端框架**：原生 HTML/CSS/JS + CodeMirror 6（CDN 加载），部署即用。
- **bcrypt 密码 + HttpOnly Cookie**：基础安全配置齐全。
- **管理员后台**：可直接在网页上新增、编辑、删除题目与测试用例。

---

## 功能一览

### 用户系统
- 账号注册（密码 bcrypt 哈希存储）
- 账号登录（HttpOnly Session Cookie）
- 登出
- 登录状态校验中间件

### 题目系统
- 分页题目列表（每页 20 题）
- 难度筛选（简单/中等/困难）
- 分类标签筛选
- 标题模糊搜索
- Markdown 题目描述
- CodeMirror 6 代码编辑器（C++ 语法高亮）
- 草稿自动保存（LocalStorage，30s）
- `Ctrl+S` 快捷提交

### 测试用例管理
- 管理员上传 `.in/.out` 测试点（≤ 20 个/题，≤ 10 MB/文件）
- 测试点关联到题目
- 删除测试点

### 评测系统
- 异步评测队列（`judge_queue` 表 + 多 worker 线程）
- g++ `-O2` 编译
- cgroup v1 资源隔离（CPU/内存/PID）
- 详细结果存储：每测试点状态、耗时、内存、实际输出 vs 期望输出

### 提交系统
- 代码提交 API
- 提交历史列表
- 提交详情（每个测试点结果对比）
- 评测状态实时反馈

### 管理后台
- 题目列表（搜索/筛选）
- 题目新增/编辑（含 Markdown 描述、标签、限制）
- 测试用例上传/删除

---

## 技术栈

| 层级 | 选型 |
|------|------|
| 后端语言 | C++ (C++20) |
| HTTP 框架 | [cpp-httplib](https://github.com/yhirose/cpp-httplib) |
| 数据库 | MySQL 5.7+ / 8.0 |
| MySQL 客户端 | libmysqlclient |
| 密码加密 | bcrypt（基于 OpenSSL） |
| 代码执行隔离 | Linux cgroup v1（CPU/内存/PID） |
| 进程隔离 | UTS/PID Namespace |
| 前端 | 原生 HTML/CSS/JavaScript |
| 代码编辑器 | CodeMirror 6（CDN） |
| 构建工具 | CMake 3.10+ |
| 单元测试 | GoogleTest |

---

## 快速开始

> 一台干净的 Ubuntu 22.04 LTS 即可。完整部署文档请见 [DEPLOY.md](./DEPLOY.md)。

```bash
# 1. 克隆
git clone <your-repo-url> cpp-oj-vibecoding
cd cpp-oj-vibecoding

# 2. 安装依赖
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libssl-dev \
                    mysql-server libmysqlclient-dev

# 3. 获取 cpp-httplib
mkdir -p third_party/httplib
curl -L -o third_party/httplib/httplib.h \
    https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
# 或者使用仓库内已有的 src/utils/httplib.h

# 4. 初始化数据库
mysql -u root -e "CREATE DATABASE IF NOT EXISTS oj_system \
    CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
mysql -u root oj_system < database/schema.sql

# 5. 修改 config/config.yaml 中的数据库密码

# 6. 编译
mkdir -p build && cd build
cmake ..
make -j$(nproc)
cd ..

# 7. 创建管理员
./build/bin/create_admin ./config/config.yaml
# 默认账号 admin / admin123（请尽快修改）

# 8. 启动
mkdir -p logs
nohup ./build/bin/oj_server     ./config/config.yaml > logs/oj_server.out 2>&1 &
nohup ./build/bin/judge_worker  ./config/config.yaml > logs/judge_worker.out 2>&1 &

# 9. 访问
open http://localhost:8080/
```

---

## 目录结构

```
cpp-oj-vibecoding/
├── CMakeLists.txt                  # 构建配置
├── README.md                       # 本文件
├── DEPLOY.md                       # 部署文档
├── API.md                          # 接口文档
├── spec.md                         # 规格说明书
├── dependence.md                   # 依赖安装指引
├── config/
│   └── config.yaml                 # 应用配置
├── database/
│   └── schema.sql                  # 数据库 Schema
├── src/
│   ├── main.cpp                    # HTTP 服务入口
│   ├── tools/
│   │   ├── create_admin.cpp        # 创建管理员
│   │   ├── reset_db.cpp            # 重置数据库
│   │   └── main_worker.cpp         # Worker 入口
│   ├── server/                     # HTTP 服务层
│   │   ├── server.{h,cpp}          # 服务类
│   │   ├── router/                 # 路由注册
│   │   ├── handlers/               # 请求处理器
│   │   └── middleware/             # 中间件
│   ├── models/                     # ORM 模型层
│   │   ├── user.{h,cpp}
│   │   ├── problem.{h,cpp}
│   │   ├── test_case.{h,cpp}
│   │   ├── submission.{h,cpp}
│   │   ├── submission_result.{h,cpp}
│   │   └── judge_queue.{h,cpp}
│   ├── services/                   # 业务服务层
│   │   ├── db_service.{h,cpp}      # 数据库封装
│   │   ├── judge_service.{h,cpp}   # 评测业务
│   │   └── cgroup_manager.{h,cpp}  # cgroup 隔离
│   ├── worker/                     # 评测 worker
│   │   └── judge_worker.{h,cpp}
│   ├── db_pool/                    # 数据库连接池
│   └── utils/                      # 工具（config/logger/password/session/json/httplib）
├── web/                            # 前端
│   ├── index.html                  # 首页
│   ├── login.html / register.html / account.html
│   ├── problem_list.html           # 题目列表
│   ├── problem.html                # 题目详情 + 编辑器
│   ├── submissions.html            # 提交历史
│   ├── admin/                      # 管理后台
│   ├── css/style.css
│   └── js/                         # api / auth / editor / problems / submission
├── uploads/                        # 上传文件目录
├── test_cases/                     # 测试用例目录
├── logs/                           # 运行日志
└── tests/unit/                     # GoogleTest 单元测试
```

---

## 核心模块

### HTTP 服务 (`src/server`)
- 基于 cpp-httplib 提供单线程阻塞 I/O + 工作线程模式。
- 路由分发见 `Router::registerHandlers()`。

### 评测 Worker (`src/worker`)
- 独立的进程/线程组，多个 worker 并行从 `judge_queue` 表拉取任务。
- 调用 `JudgeService` 完成编译→运行→判定全流程。

### Cgroup 隔离 (`src/services/cgroup_manager`)
- 通过 `cpu,cpuacct` / `memory` / `pids` cgroup 限制：
  - **CPU 时间**：通过 `cpu.cfs_quota_us`
  - **内存**：`memory.limit_in_bytes`
  - **进程数**：`pids.max`
- 启动被评测程序前调用 `addTask` 把 PID 绑定到 cgroup。
- cgroup 不可用时自动 fallback 到 `setrlimit`，仍然可工作。

### 连接池 (`src/db_pool`)
- 单例 `ConnectionPool`，最大 10 个连接（主服务）/ 5 个连接（worker），FIFO 获取。
- 内部封装 `mysql_*` C API。

### 模型层 (`src/models`)
- 每个表对应一个模型类（`User`、`Problem`、`TestCase`、`Submission`、`SubmissionResult`、`JudgeQueue`），暴露 `findByXxx / create / update / remove` 方法。

### Session (`src/utils/session.h`)
- 基于 OpenSSL 随机生成的 token，存储于服务端 `unordered_map<token, user_id>`，通过 `HttpOnly` Cookie 下发。

---

## API 概览

完整文档请参考 [API.md](./API.md)。

| 方法 | 路径 | 描述 | 权限 |
|------|------|------|------|
| GET  | `/health` | 健康检查 | 公开 |
| POST | `/api/auth/register` | 注册 | 公开 |
| POST | `/api/auth/login` | 登录 | 公开 |
| POST | `/api/auth/logout` | 登出 | 已登录 |
| GET  | `/api/auth/me` | 当前用户 | 已登录 |
| GET  | `/api/problems` | 题目列表（分页/筛选） | 公开 |
| GET  | `/api/problems/:id` | 题目详情 | 公开 |
| POST | `/api/problems` | 新增题目 | admin |
| PUT  | `/api/problems/:id` | 修改题目 | admin |
| DELETE | `/api/problems/:id` | 删除题目 | admin |
| POST | `/api/problems/:id/testcases` | 上传测试用例 | admin |
| DELETE | `/api/testcases/:id` | 删除测试用例 | admin |
| POST | `/api/submissions` | 提交评测 | user |
| GET  | `/api/submissions` | 我的提交 | user |
| GET  | `/api/submissions/:id` | 提交详情 | user |

---

## 数据库 Schema

完整 SQL 见 [`database/schema.sql`](./database/schema.sql)。共 6 张表：

| 表名 | 说明 |
|------|------|
| `users` | 用户账号与角色 |
| `problems` | 题目元数据（标题/描述/难度/标签/限制） |
| `test_cases` | 测试用例（输入输出文件路径） |
| `submissions` | 提交记录（代码/最终状态/耗时/内存） |
| `submission_results` | 每测试点的详细结果 |
| `judge_queue` | 评测任务队列（pending/running/completed/failed） |

---

## 评测状态码

| 状态码 | 含义 |
|--------|------|
| `AC`  | Accepted - 答案正确 |
| `WA`  | Wrong Answer - 答案错误 |
| `CE`  | Compilation Error - 编译错误 |
| `RE`  | Runtime Error - 运行时错误 |
| `TLE` | Time Limit Exceeded - 超时 |
| `MLE` | Memory Limit Exceeded - 内存超限 |
| `OLE` | Output Limit Exceeded - 输出超限 |
| `PE`  | Presentation Error - 格式错误 |

每个提交结果包含：每测试点的状态、耗时（ms）、内存占用（KB）、实际输出、期望输出。

---

## 安全与隔离

- **密码**：bcrypt 哈希存储，不可逆。
- **Cookie**：`HttpOnly`，降低 XSS 风险。
- **管理接口**：通过中间件校验 `role == admin`。
- **代码隔离**：
  - cgroup v1 CPU/内存/PID 限制
  - 测试用例目录禁止外部 URL 直访
  - 仅给评测进程临时读写目录
  - 可选：UST/PID Namespace（部分启用）
- **超时/超内存双保险**：除 cgroup 外，`CgroupManager` 还通过 `waitpid` + `SIGKILL` 双层保护防止子进程泄漏。

> 已知限制：当前未启用 `seccomp` 系统调用白名单，仅依赖资源隔离。如果用于不可信公网环境，建议补充 `seccomp` 或改用 Docker 容器化评测。

---

## 测试

```bash
cd build
ctest --output-on-failure
```

测试覆盖：

| 测试文件 | 覆盖点 |
|---------|--------|
| `test_config` | YAML 配置加载 |
| `test_logger` | 日志封装 |
| `test_password` | bcrypt 加解密 |
| `test_session` | Session token 生命周期 |
| `test_connection_pool` | 连接池并发获取/归还 |
| `test_models` | ORM 模型 CRUD |
| `test_models_db` | 数据库集成测试 |
| `test_auth_handler` / `test_auth_middleware` | 鉴权 |
| `test_problem_handler` | 题目接口 |
| `test_submission_handler` | 提交接口 |
| `test_testcase_handler` | 测试用例接口 |
| `test_cgroup_manager` | cgroup 资源限制 |
| `test_judge_service` | 评测流程 |

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [spec.md](./spec.md) | 完整规格说明书 |
| [DEPLOY.md](./DEPLOY.md) | 部署指南（Ubuntu 22.04） |
| [API.md](./API.md) | HTTP 接口详细文档 |
| [dependence.md](./dependence.md) | 第三方依赖说明 |

---

## 路线图

当前已实现 SPEC 中的全部 Phase 1~8，详见 [spec.md §8](./spec.md)。后续可选扩展：

- [ ] 引入 `seccomp` 系统调用白名单，进一步加固沙箱
- [ ] 支持多语言评测（Python/Java/Go）
- [ ] Docker 容器化部署与一键脚本
- [ ] 测试用例 JSON 批量导入
- [ ] 排行榜 / 比赛模式
- [ ] WebSocket 实时推送评测进度
- [ ] Prometheus 指标暴露

---

## 贡献

欢迎提交 PR 与 Issue。本项目作为教学示例，期望保持**简洁、易读、可二次开发**。

---

## 许可

仅供教学与学习使用，请遵守所在机构的实验/教学规定。