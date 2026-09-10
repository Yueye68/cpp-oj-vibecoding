# 部署文档 (DEPLOY.md)

本文档介绍如何在 Linux 环境下从零开始部署 **C++ OJ System** 在线评测系统。

> 适用平台：Ubuntu 20.04 / 22.04 / 24.04（其他 Debian 系发行版可参考）

---

## 目录

1. [环境要求](#1-环境要求)
2. [系统依赖安装](#2-系统依赖安装)
3. [数据库准备](#3-数据库准备)
4. [代码与第三方库](#4-代码与第三方库)
5. [配置文件](#5-配置文件)
6. [编译构建](#6-编译构建)
7. [初始化数据库与管理员账号](#7-初始化数据库与管理员账号)
8. [启动服务](#8-启动服务)
9. [验证部署](#9-验证部署)
10. [常见问题](#10-常见问题)
11. [卸载](#11-卸载)

---

## 1. 环境要求

| 类别 | 最低要求 | 推荐 |
|------|----------|------|
| OS | Linux (kernel ≥ 3.10) | Ubuntu 22.04 LTS |
| CPU | 2 核 | 4 核+ |
| 内存 | 2 GB | 4 GB+ |
| 磁盘 | 4 GB | 10 GB+ |
| GCC | 9.0+ | 11+ |
| CMake | 3.10+ | 3.22+ |
| MySQL | 5.7+ / 8.0 | 8.0 |
| 网络 | 评测时需联网下载第三方头文件 | - |

> 评测模块使用 **Linux cgroup v1** 进行资源隔离，建议使用 Ubuntu 22.04 及以下版本（默认 cgroup v1）。Ubuntu 23.10+ 默认 cgroup v2，可能需要额外适配。

---

## 2. 系统依赖安装

### 2.1 基础编译工具

```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config curl
```

| 包名 | 用途 |
|------|------|
| build-essential | GCC/G++、make |
| cmake | 构建系统 |
| git | 拉取 cpp-httplib |
| pkg-config | 库查找 |
| curl | 健康检查 |

### 2.2 OpenSSL 开发库

```bash
sudo apt install -y libssl-dev
```

密码 bcrypt 加密、随机数等均依赖 OpenSSL。

### 2.3 MySQL 数据库

```bash
sudo apt install -y mysql-server libmysqlclient-dev
```

| 包名 | 用途 |
|------|------|
| mysql-server | MySQL 服务端 |
| libmysqlclient-dev | C/C++ 客户端开发头文件与库 |

安装完成后启动 MySQL：

```bash
sudo systemctl enable --now mysql
sudo systemctl status mysql
```

可选：运行安全初始化向导。

```bash
sudo mysql_secure_installation
```

---

## 3. 数据库准备

### 3.1 创建数据库

```bash
mysql -u root -p << 'SQL'
CREATE DATABASE IF NOT EXISTS oj_system
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;
SQL
```

### 3.2 导入 Schema

```bash
mysql -u root -p oj_system < database/schema.sql
```

成功后应出现以下 6 张表：`users`、`problems`、`test_cases`、`submissions`、`submission_results`、`judge_queue`。

### 3.3 验证

```bash
mysql -u root -p -e "USE oj_system; SHOW TABLES;"
```

---

## 4. 代码与第三方库

### 4.1 获取项目源码

```bash
git clone <your-repo-url> cpp-oj-vibecoding
cd cpp-oj-vibecoding
```

### 4.2 获取 cpp-httplib

`cpp-httplib` 为单头文件库，已通过 `git submodule` 或下载方式提供：

```bash
# 方式 A：如果仓库包含子模块
git submodule update --init --recursive

# 方式 B：直接下载
mkdir -p third_party/httplib
curl -L -o third_party/httplib/httplib.h \
    https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

> 头文件 `httplib.h` 也可放在任意 include 路径中，CMake 已配置通过 `src/utils/httplib.h` 引用，确保该文件存在即可。

---

## 5. 配置文件

编辑 `config/config.yaml`：

```yaml
database:
  host: localhost
  port: 3306
  username: root
  password: "你的MySQL密码"     # 修改为实际密码
  name: oj_system
  charset: utf8mb4
  collation: utf8mb4_unicode_ci

server:
  host: 0.0.0.0
  port: 8080

app:
  test_case_dir: ./test_cases
  upload_dir: ./uploads
  log_level: INFO
  log_file: ./logs/app.log
  max_test_case_count: 20
  max_test_case_file_size: 10485760       # 10 MB

worker:
  poll_interval_ms: 1000
  max_retries: 3
  worker_threads: 4
  worker_id_prefix: judge_worker
```

### 5.1 配置项说明

| 段 | 字段 | 说明 |
|----|------|------|
| database | host/port/username/password/name | MySQL 连接信息 |
| server | host/port | HTTP 服务监听地址 |
| app | test_case_dir | 测试用例文件目录 |
| app | upload_dir | 用户上传文件目录 |
| app | log_level | DEBUG / INFO / WARN / ERROR |
| app | max_test_case_count | 单题测试点上限，默认 20 |
| app | max_test_case_file_size | 单文件最大字节，默认 10 MiB |
| worker | worker_threads | 评测 worker 线程数 |
| worker | poll_interval_ms | 拉取队列间隔 |

### 5.2 准备运行时目录

```bash
mkdir -p logs uploads test_cases
```

---

## 6. 编译构建

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

构建产物：

| 可执行文件 | 路径 | 说明 |
|-----------|------|------|
| `oj_server` | `build/bin/oj_server` | HTTP 主服务 |
| `judge_worker` | `build/bin/judge_worker` | 评测 worker 进程 |
| `create_admin` | `build/bin/create_admin` | 创建管理员账号 |
| `reset_db` | `build/bin/reset_db` | 清空数据库 |

> 单元测试需要先安装 gtest（可选）：
> `sudo apt install -y libgtest-dev cmake && cd build && ctest --output-on-failure`

---

## 7. 初始化数据库与管理员账号

### 7.1 创建管理员

```bash
./build/bin/create_admin ./config/config.yaml
```

成功后输出：

```
Admin user created successfully!
Username: admin
Password: admin123
```

> **生产环境请立刻修改密码！**

### 7.2 重置数据库（可选）

如需清空所有数据（危险操作）：

```bash
./build/bin/reset_db ./config/config.yaml
```

---

## 8. 启动服务

系统由两部分组成：**HTTP 服务 (`oj_server`)** 与 **评测 worker (`judge_worker`)**。

### 8.1 直接启动（开发环境）

```bash
# 终端 1：启动主服务
./build/bin/oj_server ./config/config.yaml

# 终端 2：启动评测 worker
./build/bin/judge_worker ./config/config.yaml
```

### 8.2 后台启动（推荐生产）

使用 `nohup` 或 `screen`/`tmux`：

```bash
mkdir -p logs
nohup ./build/bin/oj_server ./config/config.yaml > logs/oj_server.out 2>&1 &
echo $! > logs/oj_server.pid

nohup ./build/bin/judge_worker ./config/config.yaml > logs/judge_worker.out 2>&1 &
echo $! > logs/judge_worker.pid
```

### 8.3 使用 systemd（推荐长期运行）

创建 `/etc/systemd/system/oj-server.service`：

```ini
[Unit]
Description=C++ OJ HTTP Server
After=network.target mysql.service

[Service]
Type=simple
User=oj
WorkingDirectory=/opt/cpp-oj-vibecoding
ExecStart=/opt/cpp-oj-vibecoding/build/bin/oj_server /opt/cpp-oj-vibecoding/config/config.yaml
Restart=on-failure
RestartSec=5
StandardOutput=append:/opt/cpp-oj-vibecoding/logs/oj_server.out
StandardError=append:/opt/cpp-oj-vibecoding/logs/oj_server.err

[Install]
WantedBy=multi-user.target
```

创建 `/etc/systemd/system/oj-judge-worker.service`：

```ini
[Unit]
Description=C++ OJ Judge Worker
After=network.target mysql.service

[Service]
Type=simple
User=oj
WorkingDirectory=/opt/cpp-oj-vibecoding
ExecStart=/opt/cpp-oj-vibecoding/build/bin/judge_worker /opt/cpp-oj-vibecoding/config/config.yaml
Restart=on-failure
RestartSec=5
StandardOutput=append:/opt/cpp-oj-vibecoding/logs/judge_worker.out
StandardError=append:/opt/cpp-oj-vibecoding/logs/judge_worker.err

[Install]
WantedBy=multi-user.target
```

启用：

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now oj-server oj-judge-worker
sudo systemctl status oj-server oj-judge-worker
```

### 8.4 关闭服务

```bash
# 前台运行：Ctrl+C
# 后台运行：
kill $(cat logs/oj_server.pid)
kill $(cat logs/judge_worker.pid)

# systemd：
sudo systemctl stop oj-server oj-judge-worker
```

---

## 9. 验证部署

### 9.1 健康检查

```bash
curl -i http://localhost:8080/health
```

预期响应：`200 OK` 和正文 `OK`。

### 9.2 浏览前端

打开浏览器：

```
http://<服务器IP>:8080/
```

应能看到首页。

### 9.3 登录与提交测试

1. 访问 `/login.html`，使用 `admin / admin123` 登录。
2. 进入题目列表 `/problem_list.html`。
3. 点击任一题目，使用以下示例代码（两数之和）测试：

```cpp
#include <iostream>
int main() {
    long long a, b;
    std::cin >> a >> b;
    std::cout << (a + b) << std::endl;
    return 0;
}
```

4. 提交后等待评测，应得到 `AC`。

---

## 10. 常见问题

### Q1. 编译报 `mysql.h: No such file`

```bash
sudo apt install -y libmysqlclient-dev
```

### Q2. 启动时报 `Can't connect to MySQL`

- 确认 MySQL 已启动：`sudo systemctl status mysql`
- 确认 `config.yaml` 中的账号密码正确
- 确认 MySQL 监听 `127.0.0.1`：编辑 `/etc/mysql/mysql.conf.d/mysqld.cnf`，注释 `bind-address`

### Q3. cgroup 不可用

评测默认会尝试使用 cgroup v1 做隔离。如果系统为 cgroup v2 或无权限写入 `/sys/fs/cgroup`，会自动降级为 `rlimit` 方案，功能正常但无强隔离。检查：

```bash
ls /sys/fs/cgroup/cpu,cpuacct/   # cgroup v1
mount | grep cgroup              # 查看挂载
```

如需启用 cgroup：

```bash
sudo chmod -R a+rw /sys/fs/cgroup/cpu,cpuacct
sudo chmod -R a+rw /sys/fs/cgroup/memory
sudo chmod -R a+rw /sys/fs/cgroup/pids
```

### Q4. 端口被占用

修改 `config/config.yaml` 中 `server.port`，或：

```bash
sudo lsof -i :8080
sudo kill <PID>
```

### Q5. 中文/特殊字符乱码

确认数据库与表均为 `utf8mb4`：

```sql
ALTER DATABASE oj_system CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
ALTER TABLE problems CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
```

### Q6. 单元测试无法运行

```bash
sudo apt install -y libgtest-dev
cd build && cmake .. && make
ctest --output-on-failure
```

---

## 11. 卸载

```bash
sudo systemctl stop oj-server oj-judge-worker
sudo systemctl disable oj-server oj-judge-worker
sudo rm /etc/systemd/system/oj-server.service /etc/systemd/system/oj-judge-worker.service
sudo systemctl daemon-reload

# 删除数据库
mysql -u root -p -e "DROP DATABASE IF EXISTS oj_system;"

# 删除项目
sudo rm -rf /opt/cpp-oj-vibecoding
```

---

## 附：部署架构图

```
                         ┌──────────────────────┐
                         │   浏览器 (学生/教师)   │
                         └──────────┬───────────┘
                                    │ HTTP
                                    ▼
                         ┌──────────────────────┐
                         │  oj_server :8080     │
                         │  (cpp-httplib)        │
                         └─────┬────────┬────────┘
                               │        │
                  MySQL ◄──────┘        └──────► MySQL
                  (oj_system DB)              (judge_queue)
                               │
                               │ enqueue
                               ▼
                         ┌──────────────────────┐
                         │  judge_worker 进程    │
                         │  (cgroup 隔离编译/运行)│
                         └──────────────────────┘
```

部署完成后即可访问 `http://<server-ip>:8080/` 体验 OJ 平台。