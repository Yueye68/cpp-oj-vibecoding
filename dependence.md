# OJ 系统依赖安装指南

本文档适用于空白 Ubuntu 24.04 系统。

## 1. 系统基础依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libssl-dev
```

| 包名 | 说明 |
|------|------|
| build-essential | GCC/G++ 编译器、make 等构建工具 |
| cmake | CMake 构建系统 |
| git | 版本控制 |
| pkg-config | 库依赖查找工具 |
| libssl-dev | OpenSSL 开发库（用于 HTTPS 和加密） |

---

## 2. 数据库

### MySQL Server

```bash
sudo apt install -y mysql-server libmysqlclient-dev
```

| 包名 | 说明 |
|------|------|
| mysql-server | MySQL 数据库服务 |
| libmysqlclient-dev | MySQL C 客户端开发库 |

### MySQL 初始化配置

```bash
sudo mysql_secure_installation
sudo mysql -u root -p < database/schema.sql
```

---

## 3. C++ 第三方库

### 3.1 cpp-httplib

cpp-httplib 是单头文件库，直接下载放入项目目录即可。

```bash
# 下载 httplib.h 到第三方库目录
mkdir -p third_party/httplib
wget -O third_party/httplib/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

或使用 git clone：

```bash
git clone https://github.com/yhirose/cpp-httplib.git third_party/httplib
```

### 3.2 bcrypt

使用 OpenSSL 实现 bcrypt 加密（libssl-dev 已提供）。

### 3.3 MySQL Connector

使用 libmysqlclient-dev（已安装），CMake 会自动找到。

---

## 4. 编译构建

### 4.1 CMake 配置

```bash
mkdir -p build && cd build
cmake ..
```

### 4.2 编译

```bash
make -j$(nproc)
```

---

## 5. 前端依赖

前端使用原生 HTML/CSS/JS，无框架依赖。

CodeMirror 6 代码编辑器通过 CDN 动态加载，无需本地安装。

---

## 6. 快速安装脚本（完整）

```bash
#!/bin/bash
set -e

echo "=== 安装系统基础依赖 ==="
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libssl-dev

echo "=== 安装 MySQL ==="
sudo apt install -y mysql-server libmysqlclient-dev

echo "=== 下载 cpp-httplib ==="
mkdir -p third_party
git clone https://github.com/yhirose/cpp-httplib.git third_party/httplib

echo "=== 依赖安装完成 ==="
```

---

## 7. 目录结构（第三方库）

```
third_party/
└── httplib/          # cpp-httplib HTTP 库
    └── httplib.h
```

---

## 8. 依赖总览

| 类别 | 依赖 | 安装方式 |
|------|------|----------|
| 系统 | build-essential, cmake, git, pkg-config, libssl-dev | apt |
| 数据库 | mysql-server, libmysqlclient-dev | apt |
| HTTP 框架 | cpp-httplib | git clone |
| 密码加密 | OpenSSL (libssl-dev) | apt（已装） |
| 前端 | 无（原生 HTML/CSS/JS + CDN） | - |
