# OJ 在线评测系统 - 基于 cURL 的接口自动化测试文档

## 概述

本文档用于验证 OJ 在线评测系统 API 的功能完整性。测试基于 cURL 命令，覆盖所有接口。

- **基础路径**: `http://localhost:8080`
- **认证方式**: Session Token (存储于 HttpOnly Cookie)
- **字符编码**: UTF-8

---

## 1. 健康检查

### GET /health

```bash
curl -s http://localhost:8080/health
```

**预期响应**: `OK`

---

## 2. 认证相关

### 2.1 用户注册

```bash
curl -s -X POST http://localhost:8080/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"123456"}' \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `201 Created`

### 2.2 用户登录

```bash
curl -s -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"123456"}' \
  -c /tmp/cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`，返回用户信息并保存 Cookie

### 2.3 获取当前用户信息

```bash
curl -s http://localhost:8080/api/auth/me \
  -b /tmp/cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

### 2.4 用户登出

```bash
curl -s -X POST http://localhost:8080/api/auth/logout \
  -b /tmp/cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

### 2.5 错误测试 - 密码错误

```bash
curl -s -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"wrongpass"}' \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `401 Unauthorized`

---

## 3. 管理员接口

### 3.1 管理员登录

```bash
curl -s -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}' \
  -c /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`，role 应为 `"admin"`

### 3.2 创建题目

```bash
curl -s -X POST http://localhost:8080/api/problems \
  -H "Content-Type: application/json" \
  -b /tmp/admin_cookies.txt \
  -d '{
    "title": "两数之和",
    "description": "给定一个整数数组nums，返回满足条件的两个数的下标",
    "difficulty": "easy",
    "tags": ["数组", "哈希表"],
    "time_limit_ms": 1000,
    "memory_limit_mb": 256
  }' \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `201 Created`，返回 `{ "message": "Problem created successfully", "id": <id> }`

### 3.3 获取题目详情

```bash
curl -s http://localhost:8080/api/problems/<ID> \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

### 3.4 获取题目列表

```bash
curl -s "http://localhost:8080/api/problems?page=1&pageSize=10" \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

### 3.5 更新题目

```bash
curl -s -X PUT http://localhost:8080/api/problems/<ID> \
  -H "Content-Type: application/json" \
  -b /tmp/admin_cookies.txt \
  -d '{"title": "两数之和 Updated", "difficulty": "medium"}' \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

### 3.6 删除题目

```bash
curl -s -X DELETE http://localhost:8080/api/problems/<ID> \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

### 3.7 上传测试用例

```bash
# 创建临时测试文件
echo -e "2\n3" > /tmp/test_input.txt
echo -e "5" > /tmp/test_output.txt

# 上传测试用例
curl -s -X POST http://localhost:8080/api/problems/<ID>/testcases \
  -F "input=@/tmp/test_input.txt" \
  -F "output=@/tmp/test_output.txt" \
  -F "is_sample=1" \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `201 Created`

### 3.8 获取测试用例列表

```bash
curl -s http://localhost:8080/api/problems/<ID>/testcases \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

### 3.9 删除测试用例

```bash
curl -s -X DELETE http://localhost:8080/api/testcases/<TESTCASE_ID> \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

---

## 4. 提交相关

### 4.1 提交代码评测

```bash
curl -s -X POST http://localhost:8080/api/submissions \
  -H "Content-Type: application/json" \
  -b /tmp/admin_cookies.txt \
  -d '{
    "problem_id": <ID>,
    "code": "#include <bits/stdc++.h>\nusing namespace std;\nint main() { return 0; }",
    "language": "cpp"
  }' \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `201 Created`

### 4.2 获取提交历史

```bash
curl -s http://localhost:8080/api/submissions \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

### 4.3 获取提交详情

```bash
curl -s http://localhost:8080/api/submissions/<SUBMISSION_ID> \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

---

## 5. 错误响应测试

### 5.1 题目不存在

```bash
curl -s http://localhost:8080/api/problems/99999 \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `404 Not Found`

### 5.2 非管理员创建题目

```bash
# 先用普通用户登录
curl -s -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"123456"}' \
  -c /tmp/cookies.txt

# 尝试创建题目
curl -s -X POST http://localhost:8080/api/problems \
  -H "Content-Type: application/json" \
  -b /tmp/cookies.txt \
  -d '{"title":"Test","difficulty":"easy"}' \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `403 Forbidden`

### 5.3 删除不存在的资源

```bash
curl -s -X DELETE http://localhost:8080/api/problems/99999 \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"

curl -s -X DELETE http://localhost:8080/api/testcases/99999 \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `404 Not Found`

---

## 6. 完整测试脚本

```bash
#!/bin/bash

BASE_URL="http://localhost:8080"
COOKIE_FILE="/tmp/test_cookies.txt"
ADMIN_COOKIE_FILE="/tmp/admin_cookies.txt"

echo "===== OJ 系统接口自动化测试 ====="

# 清理函数
cleanup() {
  rm -f "$COOKIE_FILE" "$ADMIN_COOKIE_FILE"
  rm -f /tmp/test_input.txt /tmp/test_output.txt
}

cleanup

# 1. 健康检查
echo -e "\n[1] 健康检查"
curl -s "$BASE_URL/health"
echo ""

# 2. 普通用户注册登录
echo -e "\n[2] 普通用户注册"
curl -s -X POST "$BASE_URL/api/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"123456"}'
echo ""

echo -e "\n[3] 普通用户登录"
curl -s -X POST "$BASE_URL/api/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"123456"}' \
  -c "$COOKIE_FILE"
echo ""

# 3. 管理员登录
echo -e "\n[4] 管理员登录"
curl -s -X POST "$BASE_URL/api/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}' \
  -c "$ADMIN_COOKIE_FILE"
echo ""

# 4. 创建题目
echo -e "\n[5] 创建题目"
RESPONSE=$(curl -s -X POST "$BASE_URL/api/problems" \
  -H "Content-Type: application/json" \
  -b "$ADMIN_COOKIE_FILE" \
  -d '{
    "title": "两数之和",
    "description": "给定一个整数数组",
    "difficulty": "easy",
    "tags": ["数组"],
    "time_limit_ms": 1000,
    "memory_limit_mb": 256
  }')
echo "$RESPONSE"
PROBLEM_ID=$(echo "$RESPONSE" | grep -o '"id":[0-9]*' | cut -d: -f2)

# 5. 获取题目列表
echo -e "\n[6] 获取题目列表"
curl -s "$BASE_URL/api/problems?page=1&pageSize=10"
echo ""

# 6. 获取题目详情
echo -e "\n[7] 获取题目详情"
curl -s "$BASE_URL/api/problems/$PROBLEM_ID" -b "$ADMIN_COOKIE_FILE"
echo ""

# 7. 上传测试用例
echo -e "\n[8] 上传测试用例"
echo -e "2\n3" > /tmp/test_input.txt
echo -e "5" > /tmp/test_output.txt
curl -s -X POST "$BASE_URL/api/problems/$PROBLEM_ID/testcases" \
  -F "input=@/tmp/test_input.txt" \
  -F "output=@/tmp/test_output.txt" \
  -F "is_sample=1" \
  -b "$ADMIN_COOKIE_FILE"
echo ""

# 8. 获取测试用例列表
echo -e "\n[9] 获取测试用例列表"
curl -s "$BASE_URL/api/problems/$PROBLEM_ID/testcases" -b "$ADMIN_COOKIE_FILE"
echo ""

# 9. 更新题目
echo -e "\n[10] 更新题目"
curl -s -X PUT "$BASE_URL/api/problems/$PROBLEM_ID" \
  -H "Content-Type: application/json" \
  -b "$ADMIN_COOKIE_FILE" \
  -d '{"difficulty": "medium"}'
echo ""

# 10. 提交代码
echo -e "\n[11] 提交代码"
SUBMISSION_RESP=$(curl -s -X POST "$BASE_URL/api/submissions" \
  -H "Content-Type: application/json" \
  -b "$COOKIE_FILE" \
  -d "{
    \"problem_id\": $PROBLEM_ID,
    \"code\": \"#include <iostream>\",
    \"language\": \"cpp\"
  }")
echo "$SUBMISSION_RESP"
echo ""

# 11. 获取提交历史
echo -e "\n[12] 获取提交历史"
curl -s "$BASE_URL/api/submissions" -b "$COOKIE_FILE"
echo ""

# 12. 删除题目
echo -e "\n[13] 删除题目"
curl -s -X DELETE "$BASE_URL/api/problems/$PROBLEM_ID" -b "$ADMIN_COOKIE_FILE"
echo ""

# 13. 登出
echo -e "\n[14] 用户登出"
curl -s -X POST "$BASE_URL/api/auth/logout" -b "$COOKIE_FILE"
echo ""

cleanup
echo -e "\n===== 测试完成 ====="
```

---

## 7. 测试结果速查表

| 接口 | 方法 | 路径 | 预期状态码 |
|------|------|------|------------|
| 健康检查 | GET | /health | 200 |
| 注册 | POST | /api/auth/register | 201 |
| 登录 | POST | /api/auth/login | 200 |
| 登出 | POST | /api/auth/logout | 200 |
| 当前用户 | GET | /api/auth/me | 200 |
| 题目列表 | GET | /api/problems | 200 |
| 题目详情 | GET | /api/problems/:id | 200 / 404 |
| 创建题目 | POST | /api/problems | 201 / 401 / 403 |
| 更新题目 | PUT | /api/problems/:id | 200 / 404 |
| 删除题目 | DELETE | /api/problems/:id | 200 / 404 |
| 上传测试用例 | POST | /api/problems/:id/testcases | 201 / 404 |
| 测试用例列表 | GET | /api/problems/:id/testcases | 200 / 404 |
| 删除测试用例 | DELETE | /api/testcases/:id | 200 / 404 |
| 提交代码 | POST | /api/submissions | 201 / 401 / 404 |
| 提交历史 | GET | /api/submissions | 200 / 401 |
| 提交详情 | GET | /api/submissions/:id | 200 / 401 / 403 / 404 |

---

## 8. 常见问题排查

1. **Server not running**: 启动服务器 `./build/bin/oj_server`
2. **401 Unauthorized**: 检查 Cookie 是否正确保存和传递
3. **403 Forbidden**: 确认使用的是管理员账号 admin/admin123
4. **404 Not Found**: 确认资源 ID 存在
