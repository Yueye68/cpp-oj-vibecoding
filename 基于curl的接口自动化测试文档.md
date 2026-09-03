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

> **注意**: 评测系统采用异步队列模式，提交代码后立即返回 `pending` 状态，需要轮询提交详情获取实际评测结果。

### 4.1 提交代码评测（异步）

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
```json
{
  "message": "Submission created and queued for judging",
  "id": 123,
  "status": "pending",
  "queue_status": "pending"
}
```

> **说明**: 响应中的 `status: "pending"` 表示提交已入队等待评测，实际结果需要通过 4.3 接口轮询获取。

### 4.2 获取提交历史

```bash
curl -s http://localhost:8080/api/submissions \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`

> **说明**: 列表中每个提交包含 `queue_status` 字段，可能值为: `pending`(等待中)、`running`(评测中)、`completed`(已完成)、`failed`(失败)

### 4.3 获取提交详情

```bash
curl -s http://localhost:8080/api/submissions/<SUBMISSION_ID> \
  -b /tmp/admin_cookies.txt \
  -w "\nHTTP Status: %{http_code}\n"
```

**预期响应**: `200 OK`
```json
{
  "id": 123,
  "user_id": 1,
  "problem_id": 1,
  "code": "#include <bits/stdc++.h>...",
  "language": "cpp",
  "status": "AC",
  "queue_status": "completed",
  "execute_time_ms": 15,
  "execute_memory_kb": 2048,
  "created_at": "2026-09-03 14:00:00",
  "results": [
    {
      "id": 1,
      "test_case_id": 1,
      "status": "AC",
      "actual_output": "5",
      "expected_output": "5",
      "execute_time_ms": 10,
      "execute_memory_kb": 1024
    }
  ]
}
```

> **轮询建议**: 提交后建议每 1-2 秒轮询一次，当 `queue_status` 变为 `completed` 或 `failed` 时表示评测结束，此时 `status` 字段为最终结果。

### 4.4 轮询获取评测结果示例

```bash
# 提交代码
SUBMISSION_RESP=$(curl -s -X POST http://localhost:8080/api/submissions \
  -H "Content-Type: application/json" \
  -b /tmp/admin_cookies.txt \
  -d '{"problem_id": <ID>, "code": "#include <bits/stdc++.h>...", "language": "cpp"}')
echo "$SUBMISSION_RESP"

SUBMISSION_ID=$(echo "$SUBMISSION_RESP" | grep -o '"id":[0-9]*' | cut -d: -f2)

# 轮询直到评测完成
while true; do
  RESP=$(curl -s http://localhost:8080/api/submissions/$SUBMISSION_ID -b /tmp/admin_cookies.txt)
  QUEUE_STATUS=$(echo "$RESP" | grep -o '"queue_status":"[^"]*"' | cut -d'"' -f4)
  echo "Queue status: $QUEUE_STATUS"
  if [ "$QUEUE_STATUS" = "completed" ] || [ "$QUEUE_STATUS" = "failed" ]; then
    echo "$RESP"
    break
  fi
  sleep 1
done
```

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

# 10. 提交代码（异步）
echo -e "\n[11] 提交代码（异步）"
SUBMISSION_RESP=$(curl -s -X POST "$BASE_URL/api/submissions" \
  -H "Content-Type: application/json" \
  -b "$COOKIE_FILE" \
  -d "{
    \"problem_id\": $PROBLEM_ID,
    \"code\": \"#include <iostream>\",
    \"language\": \"cpp\"
  }")
echo "$SUBMISSION_RESP"
SUBMISSION_ID=$(echo "$SUBMISSION_RESP" | grep -o '"id":[0-9]*' | cut -d: -f2)
echo ""

# 11. 轮询获取评测结果
echo -e "\n[12] 轮询获取评测结果"
if [ -n "$SUBMISSION_ID" ]; then
  for i in {1..10}; do
    DETAIL=$(curl -s "$BASE_URL/api/submissions/$SUBMISSION_ID" -b "$COOKIE_FILE")
    QUEUE_STATUS=$(echo "$DETAIL" | grep -o '"queue_status":"[^"]*"' | cut -d'"' -f4)
    FINAL_STATUS=$(echo "$DETAIL" | grep -o '"status":"[^"]*"' | head -1 | cut -d'"' -f4)
    echo "Attempt $i: queue_status=$QUEUE_STATUS, status=$FINAL_STATUS"
    if [ "$QUEUE_STATUS" = "completed" ] || [ "$QUEUE_STATUS" = "failed" ]; then
      echo "Result: $DETAIL"
      break
    fi
    sleep 1
  done
fi
echo ""

# 12. 获取提交历史
echo -e "\n[13] 获取提交历史"
curl -s "$BASE_URL/api/submissions" -b "$COOKIE_FILE"
echo ""

# 13. 删除题目
echo -e "\n[14] 删除题目"
curl -s -X DELETE "$BASE_URL/api/problems/$PROBLEM_ID" -b "$ADMIN_COOKIE_FILE"
echo ""

# 14. 登出
echo -e "\n[15] 用户登出"
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

**提交相关状态说明**:
- `queue_status`: 队列状态 - `pending`(等待中)、`running`(评测中)、`completed`(完成)、`failed`(失败)
- `status`: 评测结果 - `AC`(通过)、`WA`(答案错误)、`CE`(编译错误)、`TLE`(超时)、`MLE`(超内存)、`RE`(运行错误)

---

## 8. 常见问题排查

1. **Server not running**: 启动服务器 `./build/bin/oj_server`
2. **401 Unauthorized**: 检查 Cookie 是否正确保存和传递
3. **403 Forbidden**: 确认使用的是管理员账号 admin/admin123
4. **404 Not Found**: 确认资源 ID 存在
5. **提交后 status 一直是 pending**: 确认 `judge_worker` 进程正在运行，评测服务需要单独启动:
   ```bash
   # 启动 Worker
   ./build/bin/judge_worker ./config/config.yaml
   ```
