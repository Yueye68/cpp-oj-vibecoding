# OJ 在线评测系统 API 文档

## 概述

- **基础路径**: `http://{host}:{port}` (默认 `http://localhost:8080`)
- **认证方式**: Session Token (存储于 HttpOnly Cookie)
- **默认端口**: 8080
- **字符编码**: UTF-8

---

## 目录

- [健康检查](#1-健康检查)
- [认证相关](#2-认证相关)
- [题目相关](#3-题目相关)
- [测试用例相关](#4-测试用例相关)
- [提交相关](#5-提交相关)

---

## 1. 健康检查

### GET /health

服务器健康状态检查。

**参数**: 无

**响应**: `200 OK`
```
OK
```

---

## 2. 认证相关

### POST /api/auth/register

用户注册。

**参数** (Body):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| username | string | 是 | 用户名，VARCHAR(50)，唯一 |
| password | string | 是 | 密码，6-20字符 |

**响应**: `201 Created`
```json
{
  "message": "User registered successfully"
}
```

**错误响应**:
- `400 Bad Request`: 参数缺失或无效
- `409 Conflict`: 用户名已存在

---

### POST /api/auth/login

用户登录。

**参数** (Body):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| username | string | 是 | 用户名 |
| password | string | 是 | 密码 |

**响应**: `200 OK`
```json
{
  "message": "Login successful",
  "user": {
    "id": 1,
    "username": "string",
    "role": "user"
  }
}
```

**Cookie**: 登录成功后设置 `session_token` HttpOnly Cookie

**错误响应**:
- `400 Bad Request`: 参数缺失
- `401 Unauthorized`: 用户名或密码错误

---

### POST /api/auth/logout

用户登出。

**参数**: 无

**响应**: `200 OK`
```json
{
  "message": "Logout successful"
}
```

**Cookie**: 清除 `session_token` Cookie

---

### GET /api/auth/me

获取当前登录用户信息。

**参数**: 无

**请求头**: 自动读取 Cookie 中的 `session_token`

**响应**: `200 OK`
```json
{
  "id": 1,
  "username": "string",
  "role": "user",
  "created_at": "2024-01-01T00:00:00Z"
}
```

**错误响应**:
- `401 Unauthorized`: 未登录或会话失效

---

## 3. 题目相关

### GET /api/problems

题目列表（分页、筛选）。

**参数** (Query):
| 参数名 | 类型 | 必填 | 默认值 | 说明 |
|--------|------|------|--------|------|
| page | int | 否 | 1 | 页码 |
| pageSize | int | 否 | 20 | 每页数量，最大100 |
| difficulty | string | 否 | - | 难度筛选: easy/medium/hard |
| search | string | 否 | - | 按标题搜索 |
| tags | string | 否 | - | 分类标签，逗号分隔 |

**响应**: `200 OK`
```json
{
  "problems": [
    {
      "id": 1,
      "title": "两数之和",
      "difficulty": "easy",
      "tags": ["数组", "哈希表"],
      "test_case_count": 5
    }
  ],
  "total": 100,
  "page": 1,
  "pageSize": 20,
  "totalPages": 5
}
```

---

### GET /api/problems/:id

题目详情。

**参数** (Path):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| id | int | 是 | 题目ID |

**响应**: `200 OK`
```json
{
  "id": 1,
  "title": "两数之和",
  "description": "给定一个整数数组 nums ...",
  "difficulty": "easy",
  "tags": ["数组", "哈希表"],
  "time_limit_ms": 1000,
  "memory_limit_mb": 256,
  "test_case_count": 5,
  "created_at": "2024-01-01T00:00:00Z",
  "updated_at": "2024-01-01T00:00:00Z"
}
```

**错误响应**:
- `404 Not Found`: 题目不存在

---

### POST /api/problems

新增题目（管理员）。

**权限**: 需要管理员登录

**参数** (Body):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| title | string | 是 | 题目标题 |
| description | string | 否 | 题目描述（Markdown） |
| difficulty | string | 是 | 难度: easy/medium/hard |
| tags | array | 否 | 分类标签数组 |
| time_limit_ms | int | 否 | 时间限制（毫秒），默认1000 |
| memory_limit_mb | int | 否 | 内存限制（MB），默认256 |

**响应**: `201 Created`
```json
{
  "message": "Problem created successfully",
  "id": 1
}
```

**错误响应**:
- `401 Unauthorized`: 未登录
- `403 Forbidden`: 非管理员

---

### PUT /api/problems/:id

修改题目（管理员）。

**权限**: 需要管理员登录

**参数** (Path):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| id | int | 是 | 题目ID |

**参数** (Body): 所有字段可选
| 参数名 | 类型 | 说明 |
|--------|------|------|
| title | string | 题目标题 |
| description | string | 题目描述 |
| difficulty | string | 难度 |
| tags | array | 分类标签 |
| time_limit_ms | int | 时间限制 |
| memory_limit_mb | int | 内存限制 |

**响应**: `200 OK`
```json
{
  "message": "Problem updated successfully"
}
```

**错误响应**:
- `401 Unauthorized`: 未登录
- `403 Forbidden`: 非管理员
- `404 Not Found`: 题目不存在

---

### DELETE /api/problems/:id

删除题目（管理员）。

**权限**: 需要管理员登录

**参数** (Path):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| id | int | 是 | 题目ID |

**响应**: `200 OK`
```json
{
  "message": "Problem deleted successfully"
}
```

**错误响应**:
- `401 Unauthorized`: 未登录
- `403 Forbidden`: 非管理员
- `404 Not Found`: 题目不存在

---

## 4. 测试用例相关

### POST /api/problems/:id/testcases

上传测试用例（管理员）。

**权限**: 需要管理员登录

**参数** (Path):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| id | int | 是 | 题目ID |

**参数** (Form Data):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| input | file | 是 | 输入文件（最大10MB） |
| output | file | 是 | 输出文件（最大10MB） |
| is_sample | int | 否 | 是否为示例测试点（0或1），默认0 |

**响应**: `201 Created`
```json
{
  "message": "Test case added successfully",
  "id": 1,
  "input_path": "/uploads/test_cases/...",
  "output_path": "/uploads/test_cases/...",
  "is_sample": 0
}
```

**错误响应**:
- `401 Unauthorized`: 未登录
- `403 Forbidden`: 非管理员
- `404 Not Found`: 题目不存在

---

### GET /api/problems/:id/testcases

获取题目的测试用例列表。

**参数** (Path):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| id | int | 是 | 题目ID |

**响应**: `200 OK`
```json
{
  "test_cases": [
    {
      "id": 1,
      "problem_id": 1,
      "score": 100,
      "is_sample": 1
    }
  ],
  "count": 5
}
```

**错误响应**:
- `404 Not Found`: 题目不存在

---

### DELETE /api/testcases/:id

删除测试用例（管理员）。

**权限**: 需要管理员登录

**参数** (Path):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| id | int | 是 | 测试用例ID |

**响应**: `200 OK`
```json
{
  "message": "Test case deleted successfully"
}
```

**错误响应**:
- `401 Unauthorized`: 未登录
- `403 Forbidden`: 非管理员
- `404 Not Found`: 测试用例不存在

---

## 5. 提交相关

### POST /api/submissions

提交代码评测。

**权限**: 需要用户登录

**参数** (Body):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| problem_id | int | 是 | 题目ID |
| code | string | 是 | 源代码 |
| language | string | 否 | 语言，默认 "cpp" |

**响应**: `201 Created`
```json
{
  "id": 1,
  "status": "AC",
  "message": "Submission received"
}
```

**评测状态码**:
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

**错误响应**:
- `401 Unauthorized`: 未登录
- `404 Not Found`: 题目不存在

---

### GET /api/submissions

获取当前用户的提交历史。

**权限**: 需要用户登录

**参数** (Query):
| 参数名 | 类型 | 必填 | 默认值 | 说明 |
|--------|------|------|--------|------|
| page | int | 否 | 1 | 页码 |
| pageSize | int | 否 | 20 | 每页数量 |

**响应**: `200 OK`
```json
{
  "submissions": [
    {
      "id": 1,
      "problem_id": 1,
      "problem_title": "两数之和",
      "status": "AC",
      "execute_time_ms": 15,
      "execute_memory_kb": 2048,
      "created_at": "2024-01-01T00:00:00Z"
    }
  ],
  "total": 50,
  "page": 1,
  "pageSize": 20,
  "totalPages": 3
}
```

**错误响应**:
- `401 Unauthorized`: 未登录

---

### GET /api/submissions/:id

获取提交详情（含各测试点结果）。

**权限**: 需要用户登录（仅能查看自己的提交）

**参数** (Path):
| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| id | int | 是 | 提交ID |

**响应**: `200 OK`
```json
{
  "id": 1,
  "problem_id": 1,
  "code": "#include <iostream>...",
  "language": "cpp",
  "status": "WA",
  "error_detail": null,
  "execute_time_ms": 15,
  "execute_memory_kb": 2048,
  "created_at": "2024-01-01T00:00:00Z",
  "results": [
    {
      "test_case_id": 1,
      "status": "AC",
      "execute_time_ms": 5,
      "execute_memory_kb": 1024,
      "expected_output": "3",
      "actual_output": "3"
    },
    {
      "test_case_id": 2,
      "status": "WA",
      "execute_time_ms": 3,
      "execute_memory_kb": 1024,
      "expected_output": "5",
      "actual_output": "4"
    }
  ]
}
```

**错误响应**:
- `401 Unauthorized`: 未登录
- `403 Forbidden`: 不是自己的提交
- `404 Not Found`: 提交不存在

---

## 附录

### 错误响应格式

所有 API 错误响应均返回 JSON：
```json
{
  "error": "错误描述信息"
}
```

### HTTP 状态码

| 状态码 | 说明 |
|--------|------|
| 200 | 成功 |
| 201 | 创建成功 |
| 400 | 请求参数错误 |
| 401 | 未认证 |
| 403 | 无权限 |
| 404 | 资源不存在 |
| 409 | 资源冲突 |
| 500 | 服务器内部错误 |
