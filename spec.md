# OJ 在线评测系统规格说明书

## 1. 项目概述

**项目名称**: C++ OJ System
**项目类型**: 教学用在线评测系统
**核心功能**: 仿 LeetCode 的编程题目在线评测平台，支持用户注册登录、题目浏览、代码提交、在线评测、题目管理等功能
**目标用户**: 教师（出题管理）、学生（刷题练习）

---

## 2. 技术栈

| 层级 | 技术选型 | 说明 |
|------|----------|------|
| 后端 | C++ + cpp-httplib | 轻量高性能 HTTP 服务框架 |
| 数据库 | MySQL | 关系型数据存储 |
| 前端 | 原生 HTML + CSS + JS | 无框架依赖 |
| 代码执行 | Linux Namespace/Cgroup | 进程级资源隔离与限制 |
| 代码编辑 | CodeMirror 6 | C++ 语法高亮编辑器 |

---

## 3. 功能需求

### 3.1 用户系统

| 功能 | 描述 | 角色 |
|------|------|------|
| 用户注册 | 账号 + 密码注册 | 所有用户 |
| 用户登录 | 账号 + 密码登录，Session 会话 | 所有用户 |
| 用户登出 | 销毁会话 | 所有用户 |

**约束**:
- 密码使用 bcrypt 加密存储
- Session 存储于服务端（内存或 Redis）
- 无邮件验证、无第三方 OAuth

### 3.2 题目系统

| 功能 | 描述 | 角色 |
|------|------|------|
| 题目列表 | 分页展示题目，含难度/分类筛选 | 所有用户 |
| 题目详情 | 查看题目描述、示例输入输出 | 所有用户 |
| 代码提交 | 在线编辑代码并提交评测 | 普通用户 |
| 提交历史 | 查看自己的提交记录及状态 | 普通用户 |

**题目元数据**:
- 标题、描述（Markdown 支持）
- 难度等级：简单 / 中等 / 困难
- 分类标签：算法、数据结构等（多选）
- 时间限制：默认 1s（可配置）
- 内存限制：默认 256MB（可配置）

**题目管理（管理员）**:
- 新增题目（标题、描述、难度、分类、测试用例）
- 删除题目
- 修改题目
- 查询题目（支持 ID / 标题搜索）

### 3.3 测试用例

| 限制项 | 数值 |
|--------|------|
| 单题测试点上限 | 20 个 |
| 单个输入文件大小上限 | 10 MB |
| 单个输出文件大小上限 | 10 MB |

**存储方式**: 文件系统存储，数据库记录路径映射

### 3.4 评测系统

**评测流程**:
1. 接收用户提交的源代码
2. 编译源代码（gcc/g++ -O2）
3. 依次运行每个测试点
4. 对比输出与期望结果
5. 返回评测结果

**结果状态码**:
| 状态码 | 含义 |
|--------|------|
| AC | 答案正确 (Accepted) |
| WA | 答案错误 (Wrong Answer) |
| CE | 编译错误 (Compilation Error) |
| RE | 运行时错误 (Runtime Error) |
| TLE | 超时 (Time Limit Exceeded) |
| MLE | 内存超限 (Memory Limit Exceeded) |
| OLE | 输出超限 (Output Limit Exceeded) |
| PE | 格式错误 (Presentation Error) |

**结果详情**:
- 状态码
- 编译错误信息及行号
- 运行错误的错误类型和位置
- 各测试点的期望输出 vs 实际输出对比
- 各测试点的执行时间/内存占用

**安全隔离（Cgroup）**:
- CPU 时间限制：通过 cgroup CPU quota 控制
- 内存限制：通过 cgroup memory.limit_in_bytes 控制
- 进程数限制：通过 cgroup pids.max 控制
- 网络隔离：禁用网络（UST Namespace）
- 文件系统限制：只读根文件系统 + 临时读写目录

### 3.5 前端页面

| 页面 | 路径 | 功能 |
|------|------|------|
| 首页 | `/` | 平台介绍、快速入口 |
| 登录页 | `/login.html` | 账号密码登录 |
| 注册页 | `/register.html` | 账号密码注册 |
| 登出页 | `/logout.html` | 销毁会话、跳转登录 |
| 题目列表页 | `/problems.html` | 题目列表、难度/分类筛选、分页 |
| 题目详情页 | `/problem.html?id={id}` | 题目描述、代码编辑器、提交按钮 |
| 提交历史页 | `/submissions.html` | 我的提交记录列表 |
| 管理后台-题目列表 | `/admin/problems.html` | 管理员题目列表、增删改查 |
| 管理后台-题目编辑 | `/admin/problem_edit.html?id={id}` | 新增/编辑题目表单 |

**页面详细功能**:

#### 首页 `/`
- 展示平台名称和简介
- 显示快捷入口（进入题目列表、登录/注册）
- 已登录用户显示用户名和登出入口

#### 登录页 `/login.html`
- 账号密码输入框
- 登录按钮
- 注册链接跳转
- 登录成功后跳转首页或上一页

#### 注册页 `/register.html`
- 用户名、密码、确认密码输入框
- 注册按钮
- 登录链接跳转
- 注册成功后跳转登录页

#### 登出页 `/logout.html`
- 清除用户 Session
- 跳转登录页并提示"已登出"

#### 题目列表页 `/problems.html`
- 题目分页列表（每页 20 题）
- 难度筛选（全部/简单/中等/困难）
- 分类标签筛选（多选）
- 搜索框（按标题搜索）
- 点击题目跳转详情页
- 导航栏显示登录/注册或用户名+登出

#### 题目详情页 `/problem.html?id={id}`
- 题目标题、难度标签、分类标签
- 题目描述（Markdown 渲染）
- 示例输入输出
- CodeMirror 代码编辑器（C++ 语法高亮）
- 提交按钮（Ctrl+S 快捷提交）
- 自动草稿保存（LocalStorage，每 30 秒）
- 提交后显示评测结果

#### 提交历史页 `/submissions.html`
- 我的提交记录列表
- 显示题目、状态、时间
- 点击跳转提交详情

#### 管理后台-题目列表 `/admin/problems.html`
- 所有题目列表（管理员视角）
- 新增题目按钮
- 编辑/删除操作
- 题目搜索

#### 管理后台-题目编辑 `/admin/problem_edit.html?id={id}`
- 新增：无 ID 参数
- 编辑：?id={problem_id}
- 表单：标题、描述、难度、分类、时间限制、内存限制
- 测试用例上传管理
- 保存/取消按钮

**编辑器功能**:
- C++ 语法高亮（CodeMirror 6）
- 自动草稿保存（LocalStorage，每 30 秒）
- 基础快捷键支持（Ctrl+S 提交）

---

## 4. 非功能需求

### 4.1 性能

| 指标 | 目标 |
|------|------|
| 并发用户数 | < 50 人同时在线 |
| 单次提交响应时间 | < 5s（不含排队） |
| 页面加载时间 | < 1s |

### 4.2 可扩展性

- 数据库schema设计支持未来增加字段
- 代码执行模块独立，支持替换为 Docker 方案

### 4.3 安全性

- 用户密码 bcrypt 加密
- Session 会话安全（HttpOnly Cookie）
- 题目管理后台需管理员权限验证
- 代码执行环境完全隔离

### 4.4 成本

- 单机部署，无需多节点
- 无外部付费服务依赖

---

## 5. 数据库Schema

```sql
-- 用户表
CREATE TABLE users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('user', 'admin') DEFAULT 'user',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 题目表
CREATE TABLE problems (
    id INT PRIMARY KEY AUTO_INCREMENT,
    title VARCHAR(200) NOT NULL,
    description TEXT,
    difficulty ENUM('easy', 'medium', 'hard') NOT NULL,
    tags JSON,
    time_limit_ms INT DEFAULT 1000,
    memory_limit_mb INT DEFAULT 256,
    test_case_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- 测试用例表
CREATE TABLE test_cases (
    id INT PRIMARY KEY AUTO_INCREMENT,
    problem_id INT NOT NULL,
    input_path VARCHAR(500) NOT NULL,
    output_path VARCHAR(500) NOT NULL,
    score INT DEFAULT 100,
    is_sample TINYINT DEFAULT 0,
    FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
);

-- 提交记录表
CREATE TABLE submissions (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL,
    problem_id INT NOT NULL,
    code TEXT NOT NULL,
    language VARCHAR(20) DEFAULT 'cpp',
    status VARCHAR(20) NOT NULL,
    error_detail TEXT,
    execute_time_ms INT,
    execute_memory_kb INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (problem_id) REFERENCES problems(id)
);

-- 提交详细结果表
CREATE TABLE submission_results (
    id INT PRIMARY KEY AUTO_INCREMENT,
    submission_id INT NOT NULL,
    test_case_id INT NOT NULL,
    status VARCHAR(20) NOT NULL,
    actual_output TEXT,
    expected_output TEXT,
    execute_time_ms INT,
    execute_memory_kb INT,
    FOREIGN KEY (submission_id) REFERENCES submissions(id) ON DELETE CASCADE,
    FOREIGN KEY (test_case_id) REFERENCES test_cases(id)
);
```

---

## 6. API 设计

### 6.1 认证相关

| 方法 | 路径 | 描述 |
|------|------|------|
| POST | /api/auth/register | 用户注册 |
| POST | /api/auth/login | 用户登录 |
| POST | /api/auth/logout | 用户登出 |
| GET | /api/auth/me | 获取当前用户信息 |

### 6.2 题目相关

| 方法 | 路径 | 描述 | 权限 |
|------|------|------|------|
| GET | /api/problems | 题目列表（分页/筛选） | 全部 |
| GET | /api/problems/:id | 题目详情 | 全部 |
| POST | /api/problems | 新增题目 | admin |
| PUT | /api/problems/:id | 修改题目 | admin |
| DELETE | /api/problems/:id | 删除题目 | admin |

### 6.3 提交相关

| 方法 | 路径 | 描述 | 权限 |
|------|------|------|------|
| POST | /api/submissions | 提交代码评测 | user |
| GET | /api/submissions | 我的提交历史 | user |
| GET | /api/submissions/:id | 提交详情（含各测试点结果） | user |

### 6.4 测试用例相关

| 方法 | 路径 | 描述 | 权限 |
|------|------|------|------|
| POST | /api/problems/:id/testcases | 上传测试用例 | admin |
| DELETE | /api/testcases/:id | 删除测试用例 | admin |

---

## 7. 目录结构

```
/home/yueye/my_dir/cpp-oj-vibecoding/
├── SPEC.md
├── README.md
├── CMakeLists.txt
├── config/
│   └── config.yaml
├── database/
│   └── schema.sql
├── src/
│   ├── main.cpp
│   ├── server/
│   │   ├── server.h
│   │   ├── server.cpp
│   │   ├── router/
│   │   │   ├── router.h
│   │   │   └── router.cpp
│   │   ├── handlers/
│   │   │   ├── auth_handler.h
│   │   │   ├── auth_handler.cpp
│   │   │   ├── problem_handler.h
│   │   │   ├── problem_handler.cpp
│   │   │   ├── submission_handler.h
│   │   │   └── submission_handler.cpp
│   │   └── middleware/
│   │       ├── auth_middleware.h
│   │       └── auth_middleware.cpp
│   ├── models/
│   │   ├── user.h
│   │   ├── user.cpp
│   │   ├── problem.h
│   │   ├── problem.cpp
│   │   ├── test_case.h
│   │   ├── test_case.cpp
│   │   ├── submission.h
│   │   └── submission.cpp
│   ├── services/
│   │   ├── db_service.h
│   │   ├── db_service.cpp
│   │   ├── judge_service.h
│   │   ├── judge_service.cpp
│   │   └── cgroup_manager.h
│   │   └── cgroup_manager.cpp
│   └── utils/
│       ├── config.h
│       ├── config.cpp
│       ├── password.h
│       └── password.cpp
├── web/
│   ├── index.html
│   ├── login.html
│   ├── register.html
│   ├── problems.html
│   ├── problem.html
│   ├── submissions.html
│   ├── admin/
│   │   ├── problems.html
│   │   └── problem_edit.html
│   ├── css/
│   │   └── style.css
│   └── js/
│       ├── api.js
│       ├── auth.js
│       ├── problems.js
│       ├── submission.js
│       └── editor.js
├── test_cases/
│   └── .gitkeep
└── uploads/
    └── .gitkeep
```

---

## 8. TODO 清单

### Phase 1: 基础设施

- [ ] 项目工程化：CMake 构建配置
- [ ] 配置文件读取：config.yaml
- [ ] 数据库连接池封装
- [ ] 数据库 Schema 初始化脚本

### Phase 2: 用户系统

- [ ] 用户注册 API + 密码加密
- [ ] 用户登录 API + Session 管理
- [ ] 用户登出 API
- [ ] 登录状态校验中间件
- [ ] 前端登录/注册页面

### Phase 3: 题目系统

- [ ] 题目 CRUD API
- [ ] 题目列表分页/筛选
- [ ] 题目详情 API
- [ ] 题目管理后台页面
- [ ] 题目新增/编辑页面

### Phase 4: 测试用例管理

- [ ] 测试用例文件上传
- [ ] 测试用例与题目关联
- [ ] 测试用例删除
- [ ] 测试用例存储（文件系统）

### Phase 5: 评测系统

- [ ] Cgroup 隔离管理器
- [ ] 代码编译服务
- [ ] 代码执行服务
- [ ] 结果判定逻辑
- [ ] 评测结果存储

### Phase 6: 提交系统

- [ ] 代码提交 API
- [ ] 异步评测队列
- [ ] 提交详情 API（含各测试点结果）
- [ ] 提交历史 API
- [ ] 前端提交历史页面

### Phase 7: 前端完善

- [ ] 代码编辑器集成（CodeMirror）
- [ ] 草稿自动保存（LocalStorage）
- [ ] 题目详情页（编辑器 + 提交）
- [ ] UI 样式美化

### Phase 8: 集成与测试

- [ ] 端到端功能测试
- [ ] 沙箱安全性验证
- [ ] 性能压测

---

## 9. 验收标准

### 9.1 功能验收

| 功能 | 验收条件 |
|------|----------|
| 用户注册 | 可注册账号，密码加密存储 |
| 用户登录 | 正确账号可登录，获取 Session |
| 题目列表 | 分页展示，难度/分类筛选生效 |
| 题目详情 | 正确显示题目描述 |
| 代码提交 | 可提交代码并触发评测 |
| 评测结果 | 返回正确/错误状态及详情 |
| 题目管理 | 管理员可增删改查题目 |
| 草稿保存 | 刷新页面代码不丢失 |

### 9.2 安全验收

| 项目 | 验收条件 |
|------|----------|
| 密码安全 | 数据库存储 bcrypt 哈希值 |
| 会话安全 | Cookie HttpOnly，防止 XSS |
| 权限控制 | 普通用户无法访问管理 API |
| 代码隔离 | 恶意代码无法影响宿主机 |

### 9.3 性能验收

| 指标 | 验收条件 |
|------|----------|
| 页面加载 | 首页/列表页 < 1s |
| 评测延迟 | 简单题目 < 3s 返回结果 |
| 并发支持 | 10 人同时提交无崩溃 |

---

## 10. 潜在风险与权衡

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| Cgroup 配置复杂 | 调试困难 | 先用简单超时机制，后续扩展 |
| MySQL 部署成本 | 增加运维负担 | 提供 Docker 一键部署 |
| 前端开发效率 | 原生 JS 维护成本高 | 复用统一工具函数和组件模式 |
| 代码评测安全性 | 可能有漏网恶意代码 | 限制系统调用白名单（seccomp） |
| 测试用例管理 | 手动上传繁琐 | 后续支持 JSON 批量导入 |
