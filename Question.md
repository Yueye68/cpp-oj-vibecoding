# cpp-oj-vibecoding 面试题库

> 共 20 题,围绕本仓库的实际代码 (oj_server / judge_worker 双进程, cgroup 沙箱, MySQL 任务队列, cpp-httplib, JSON 手写序列化, GoogleTest + pytest 双层测试) 设计。
> 每题包含: **题目 / 参考答案 / 出题意图 / 追问方向**。

---

## 一、架构设计 (4 题)

### Q1. 为什么把 `oj_server` 和 `judge_worker` 设计成两个独立进程, 而不是单个进程内多线程?

**参考答案**

二者职责完全不同, 解耦后各自可独立扩缩容、独立部署。

- `oj_server` 是 IO 密集型 (HTTP 短请求, 用户登录/查看题目/提交), 用 cpp-httplib 的线程池即可; 它的瓶颈在 DB 连接和 CPU 编译以外的网络 I/O。
- `judge_worker` 是 CPU+IO 密集型, 每个任务要做 `fork` + cgroup 设置 + 编译 + 跑测试数据, 单任务可能占用数百 MB 甚至 GB 级 cgroup 配额, 还可能 fork 炸弹 / 死循环; 与 HTTP 请求进程混在一起会互相影响。
- 两个进程天然隔离: judge_worker 崩溃不会导致 OJ 整站宕机, oj_server 也可做无状态水平扩展。
- 二者通过 **MySQL `judge_queue` 表 + `submissions` 表** 通信 (status 字段轮询 / `SELECT FOR UPDATE`), 无外部依赖 (无 Redis/RabbitMQ)。

**优劣对比**
| | 优点 | 缺点 |
|---|---|---|
| 双进程 | 隔离 / 独立扩缩容 / 一个挂了不影响另一个 | 通信延迟 ≥ 一次 DB 轮询; 部署运维复杂; 日志/监控要分别采集 |
| 单进程 | 通信最直接 (内存共享) | 一个慢任务卡住可能耗尽 HTTP 线程; worker 崩溃带走整站 |

**追问方向**: 如果改造成单进程, 如何既保留进程级隔离又避免进程间通信? 提示: 子进程 + 信号/Pipe。

---

### Q2. 为什么用 MySQL 表 (`judge_queue`) 作为任务队列, 而不用 Redis Stream / RabbitMQ / Kafka?

**参考答案**

- **依赖最简化**: 项目本身已经依赖 MySQL, 再引入 Redis/消息队列对教学型 OJ 是过度设计; 部署只需 "一台 MySQL + 两份二进制"。
- **事务一致性**: 用 `START TRANSACTION + SELECT ... FOR UPDATE + UPDATE status='running' + COMMIT` 可以**原子地抢占任务** (见 `judge_queue.cpp::popPending`), 把 "队列消费 + 状态流转" 和业务写入同一个事务, 不需要额外的 ack/重试协议。
- **可观测性**: 队列就是一张表, 用 `SELECT status, COUNT(*) FROM judge_queue GROUP BY status` 就能直接看积压; 不必维护另外一套监控。
- **数据持久化**: 任务 = submission, 重启后直接从 DB 恢复, 不丢任务。

**代价**
- 轮询延迟: 默认 worker 间隔很短, 但高 QPS 下 `SELECT FOR UPDATE` 还是有行锁竞争。
- 没有消息优先级 / 延迟队列 / 重试拓扑这些 MQ 内建能力 (项目用 `priority` 字段 + `retry_count` 字段手动实现)。
- 队列膨胀: 异常 `status='running'` 行没有 reclaim 机制 (见 Q13), 需要 DBA 介入。

**追问方向**: 如果要换 Redis, 哪些逻辑要改? `SELECT FOR UPDATE` 的语义在 Redis 里如何模拟 (`BLPOP` / `LPOP` + Lua)?

---

### Q3. Session 存在进程内存里 (SessionManager 单例), 与 Redis/JWT 相比有什么取舍?

**参考答案**

- **当前实现**: `SessionManager` 是一个 `std::unordered_map<token, SessionData>` + `std::mutex`, TTL 24h, 通过 `Set-Cookie: session_token=<int64>; HttpOnly` 下发; 每次请求由 `AuthMiddleware::requireAuth` 查表验证。
- **取舍**:
  - ✅ 简单、无外部依赖, 撤销会话很容易 (改 map)。
  - ✅ 不需要客户端处理 token, 不需要 CORS 时配 `Authorization` 头。
  - ❌ **重启即丢全部会话**, 用户全部要重新登录 (DB schema 也没建 sessions 表)。
  - ❌ **不能水平扩展**: 多实例部署时, 用户登录在 A 实例, 但请求落到 B 实例 → 401。
  - ❌ Cookie token 用 `std::to_string(uniform_int_distribution<long long>(mt19937_64))`, 只有 63 bit 熵且未做 base64/hex 编码, 可猜测空间反而比 JWT (256 bit) 大。
- **改进方向**: 短期把 sessions 表迁到 MySQL (加 token / user_id / expires_at, 在 `AuthMiddleware` 里查 DB); 长期换 JWT 或 Redis。

**追问方向**: 如果既要无状态又要可撤销, 通常怎么做? 提示: JWT + 黑名单 / 短 access + 长 refresh。

---

### Q4. `judge_service` 用 "编译 → per-test-case 执行 → 聚合 verdict" 三阶段流水线, 为什么把编译这一步拆出来, 而不每跑一个测试用例就重新编译?

**参考答案**

- **编译开销**: 一次编译可能 0.5 ~ 5s, 学生代码常 50~500 行; 一道题有 10~20 个测试用例就接近一分钟。如果每个 test case 都重新编译, 整体判题时间是现在的 10 倍以上。
- **二进制复用**: 编译产物 (`<workdir>/main`) 在所有 test case 间只生成一次, 然后每个 test case `fork` 一个新进程, `dup2(stdin, infile)` + `dup2(stdout, outfile)` 后 `exec`.
- **失败短路**: 编译失败 (CE) 直接返回, 不浪费 CPU 跑测试; 编译成功后再开 per-test fork, 任何一个测试超时 / 超内存立刻 `SIGKILL` + 记录 verdict。

**追问方向**: 编译阶段有哪些缓存策略可以进一步加速? 提示: 内容哈希 + Redis 缓存编译产物。

---

## 二、C++ 语言与设计 (4 题)

### Q5. `Logger` 是如何用 C++20 的 `std::source_location` 自动获取调用方位置的? 与 `__FILE__`/`__LINE__` 宏相比有什么好处?

**参考答案**

`Logger` 在头文件里把 `std::source_location loc = std::source_location::current()` 设为函数参数的默认值:

```cpp
void info(const std::string& msg,
          std::source_location loc = std::source_location::current());
```

调用方写 `Logger::instance().info("xxx")`, 编译器会在被调函数里捕获 **调用方** 的 file/line/column (`current()` 实际返回的是调用栈上 **最顶层** 的位置, 即调用 `Logger::info` 的那行代码)。这与 `__FILE__`/`__LINE__` 的语义等价, 但:

- ✅ 调用方不写宏, 不污染代码: `LOG_INFO("...")` vs `Logger::instance().info("..." __FILE__ __LINE__)`.
- ✅ 类型安全: `std::source_location` 是一个结构体, 可以单参传递; 不像宏展开成文本片段。
- ✅ 在模板/lambda 中也工作: 宏无法可靠获取 lambda 内部行号, `source_location` 可以。
- ❌ 编译器要求 C++20, GCC ≥ 9 / Clang ≥ 9。

**追问方向**: 这个特性 GCC/Clang/MSVC 各在哪个版本完整支持? 模板里 lambda 捕获行号会怎样?

---

### Q6. 项目里大量单例 (Config / Logger / SessionManager / ConnectionPool 等), 都是 "函数局部 static" 实现的, 为什么 C++11 之后这是线程安全的? 又有什么隐患?

**参考答案**

C++11 起, 函数局部静态 对象的初始化由编译器插入一把隐式锁 (通常等价 `std::call_once`), 标准明确保证: **多线程并发首次调用 `instance()` 时, 只有一个线程执行构造, 其他线程阻塞等待**, 因此 Meyers' Singleton 安全。

```cpp
Logger& Logger::instance() {
    static Logger inst;   // 线程安全初始化
    return inst;
}
```

**隐患**: **Static Initialization Order Fiasco** — 不同编译单元中的静态对象, 初始化顺序是未定义的; 但 **同一个函数内的 static 只在首次调用时初始化**, 这恰恰消除了跨 TU 的顺序问题, 所以本项目里这种用法是安全的。真正危险的是全局 namespace-scope 的 static 对象 (`static T g_foo;`), 本项目未使用。

真正的隐患:
- `Router` 在 init 时 `httplib::Server& svr_ = Server::instance().getServer();` —— 这要求 `Server::instance()` 在 `Router` 初始化前已经构造完成; 幸好 `Server::instance()` 本身也是函数局部 static, 调用时才构造, 不会出问题。
- 析构顺序: 进程退出时各 singleton 析构顺序也不确定, 假设 Logger 先于 ConnectionPool 析构, 最后一条 DB 报错日志会丢 (Logger 已死)。生产代码通常注册 `atexit` 或反向顺序析构。

**追问方向**: 如果某天引入 `static Config g_cfg;` 这种全局 static, 跨 TU 访问会出什么问题? 如何复现?

---

### Q7. ConnectionPool 的 `getConnection` / `returnConnection` 用 `std::mutex` + `std::condition_variable` 实现阻塞取连接, 典型实现是什么样? 需要担心"惊群"吗?

**参考答案**

经典实现:

```cpp
MYSQL* ConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]{ return !pool_.empty() || closed_; });
    if (closed_) return nullptr;
    MYSQL* c = pool_.front(); pool_.pop();
    return c;
}
void ConnectionPool::returnConnection(MYSQL* c) {
    std::lock_guard<std::mutex> lk(mu_);
    pool_.push(c);
    cv_.notify_one();        // 只唤醒一个等待者
}
```

- **是否需要惊群保护?** 在 "一个连接被释放" 场景下不会惊群, 因为 `notify_one` 只唤醒一个线程。**真正可能惊群** 的是 "批量创建 N 个新连接" (`expandPool`), 这时 `notify_all` 唤醒所有等待者, 但只有 N 个能拿到新连接, 其余继续等待。生产实现一般用 `notify_one` 循环, 或在 expandPool 时用 `notify_all` 并让多余的等待者重新 `wait`。
- **本项目**: 唤醒策略单连接 (`returnConnection` 时), 所以惊群不严重; 但批量扩张可能浪费唤醒。
- **死锁风险**: 若某线程持有连接后还调用 `getConnection` (例如嵌套 service), 会自死锁; 实际项目用 RAII guard (`ConnGuard`) 解决。

**追问方向**: 改成无锁队列 (boost::lockfree::queue / moodycamel::ConcurrentQueue) 收益有多大? 提示: mutex 在本场景 (连接池 N=10) 几乎不构成瓶颈。

---

### Q8. 项目里 `mysql_close`/`mysql_free_result` 都是手动的, 几乎每个 model 都有这种代码:

```cpp
MYSQL_RES* res = mysql_store_result(conn);
if (!res) { return false; }
MYSQL_ROW row = mysql_fetch_row(res);
... // 出错直接 return false
mysql_free_result(res);
```

每条提前 return 都是资源泄漏隐患。给出你的 RAII 改造。

**参考答案**

最简单的两个 RAII 包装:

```cpp
class MysqlResGuard {
    MYSQL_RES* res_;
public:
    explicit MysqlResGuard(MYSQL_RES* r) : res_(r) {}
    ~MysqlResGuard() { if (res_) mysql_free_result(res_); }
    MysqlResGuard(const MysqlResGuard&) = delete;
    MysqlResGuard& operator=(const MysqlResGuard&) = delete;
    MYSQL_RES* get() const { return res_; }
};

class MysqlConnGuard {
    ConnectionPool& pool_;
    MYSQL* conn_;
public:
    MysqlConnGuard(ConnectionPool& p, MYSQL* c) : pool_(p), conn_(c) {}
    ~MysqlConnGuard() { if (conn_) pool_.returnConnection(conn_); }
    MysqlConnGuard(const MysqlConnGuard&) = delete;
    MysqlConnGuard& operator=(const MysqlConnGuard&) = delete;
    MYSQL* operator->() { return conn_; }
    MYSQL* get() const { return conn_; }
};
```

调用方:
```cpp
auto cg = MysqlConnGuard(ConnectionPool::instance().getConnection());
if (!cg.get()) return false;
... // 中途任何 return / throw 都会自动归还
```

**进阶**: 把 `MYSQL_STMT*` 也用 RAII 包; 把 `cgroup_dir fd` / `tmpfile path` 都包成 guard; 配合 `std::filesystem::path` 用 `std::experimental::scope_exit` 或 C++17 之前手写 finally 模板。

**追问方向**: httplib 的 handler 抛异常时, 已经归还的连接还能正常响应吗? 提示: 全局 `set_exception_handler`。

---

## 三、数据库与 SQL (4 题)

### Q9. 项目里同时存在两套 SQL 风格: `Problem::create/update` 用预处理语句 (`mysql_stmt_*` + `MYSQL_BIND`), 其他 model 用字符串拼接 + 自定义 `escapeString` (只转义单引号)。从安全和一致性角度评价。

**参考答案**

- **预处理语句 (`Problem::create/update`)** ✅
  - 防 SQL 注入: 数据和 SQL 结构分离, 驱动层做转义。
  - 类型安全: `MYSQL_BIND` 显式声明字段类型 / 长度, 减少类型转换错误。
  - 二进制安全: 字段含 NUL 字节也没问题 (字段长度是显式给出)。
- **字符串拼接 + 自定义 escapeString** ⚠️
  - `escapeString` 只把 `'` → `\'`; **对反斜杠、双引号、NUL 字节完全没有处理**。
  - 学生提交的 `code` 字段可以含任意字符 (包括反斜杠续行符、字符串字面量中的单引号等等)。
  - 用户名/题目标签等普通字段也被该函数 "保护", 但函数实现本身是不完整的。
- **现实威胁**:
  - 用户名字段含 `'; DROP TABLE users; --` 这种经典注入, 在当前 `escapeString` 下会被 `\` 转义成 `\'; DROP TABLE users; --` → 字面字符串, 看似安全。
  - 但 `code` 字段在 `submissions` 表里是 `TEXT`, 即使含反斜杠也只会被当作数据写入, 因为拼接模板是 `INSERT INTO submissions (..., code, ...) VALUES (..., 'XXX', ...)`, 而 `XXX` 里如果含反斜杠, 反斜杠 **不会** 闭合任何 SQL 结构, 所以**不会发生注入**。
  - 但 **`LIKE` 查询** 的通配符 `%`/`_` 不被 `escapeString` 处理, 可能导致搜索/过滤出现非预期匹配 (不算注入, 但算 bug)。

**整改建议**:
1. **全局统一到预处理语句** —— 项目里 `reset_db.cpp` 已经用 `mysql_real_escape_string`, 说明作者知道更好的方案; 应当把所有 model 都迁过去, 包括 User / Submission / JudgeQueue / TestCase。
2. 或者用参数化查询模板 (类似 SQLx / sqlc 的代码生成)。
3. 给 `escapeString` 加单元测试: 注入 `'\'` / `\\` / `%` / `_` / NUL 字节 / UTF-8 多字节字符。

**追问方向**: 给一段能绕过现有 `escapeString` 的攻击 payload, 并解释为什么它对 `LIKE` 查询生效。

---

### Q10. `JudgeQueueItem::popPending` 用 `START TRANSACTION + SELECT ... FOR UPDATE + UPDATE ... SET status='running' + COMMIT` 抢占任务。MySQL InnoDB 在 RR 隔离级别下, 这里会发生什么 (gap lock / next-key lock)? 会不会死锁?

**参考答案**

```sql
START TRANSACTION;
SELECT * FROM judge_queue
 WHERE status='pending'
 ORDER BY priority DESC, created_at ASC
 LIMIT 1 FOR UPDATE;
-- 应用层拿到 row
UPDATE judge_queue SET status='running', worker_id=?, started_at=NOW()
 WHERE id=?;
COMMIT;
```

- **RR 隔离级别下**: `SELECT ... FOR UPDATE` 不仅锁住匹配行, 还会在 `status` 列的索引 (如果有) 上加 **next-key lock**, 即 "记录锁 + 间隙锁"; 防止幻读, 阻止其他事务插入新的 `pending` 行。
- **没有索引怎么办?** 如果 `status` 没有索引 (本项目 schema 没建), InnoDB 会做 **全表 next-key lock**, 即锁住整个表 (负优化); 多个 worker 并发时几乎立刻死锁或严重阻塞。
- **死锁场景**:
  - W1 拿到 id=1 的行锁; W2 等 id=1; W1 改完 id=1, COMMIT → W2 拿到, 改完 COMMIT, OK, 不死锁。
  - 但如果有两个 worker 各自按 `ORDER BY priority DESC, created_at ASC LIMIT 1` 选中 **同一行**, 二者都会阻塞等对方, 但因为是同一行, MySQL 检测到后会回滚其中一个; 用户看到的现象是偶发 "Lock wait timeout"。
- **本项目实际表现**: `created_at` 上没建索引, `status` 上没建索引, `priority` 上没建索引。InnoDB 会在主键上做 next-key lock, 仍然有锁竞争; 但只要每次只 SELECT 一行 (LIMIT 1), 锁粒度可控。
- **修复建议**:
  1. 给 `status` + `created_at` 加组合索引, 减少 gap lock 范围。
  2. 改用 `UPDATE ... SET status='running', worker_id=?, started_at=NOW() WHERE status='pending' ORDER BY priority DESC, created_at ASC LIMIT 1;` 单条 SQL (MySQL 不直接支持 LIMIT in UPDATE), 改用 `UPDATE ... WHERE id IN (SELECT id FROM ... WHERE status='pending' ORDER BY ... LIMIT 1 FOR UPDATE SKIP LOCKED);` —— **`SKIP LOCKED`** 是 MySQL 8.0 引入的关键特性, 直接跳过被锁的行, 大幅减少竞争。

**追问方向**: `SKIP LOCKED` 与 `NOWAIT` 的区别? 在 PG 12+ 也有, 实现细节是否一致?

---

### Q11. `problems.tags` 是 MySQL `JSON` 列, 但 `Problem::jsonToTags` 反序列化用 `std::regex` 提取 `"..."`; `Problem::listTagsWithCount` 用 `JSON_TABLE` 聚合。讨论 JSON 列查询的几种方案, 为什么 `JSON_CONTAINS` 和 `JSON_TABLE` 更稳?

**参考答案**

**三种 JSON 列操作方式**

1. **应用层解析 (本项目部分使用)**
   ```sql
   SELECT tags FROM problems;
   -- 应用层 std::regex 提取
   ```
   ✅ 灵活 ❌ 慢 (每行数据都要解析), 无法用索引, 数据格式错误时崩溃。

2. **`JSON_CONTAINS` 精确查询**
   ```sql
   SELECT * FROM problems
    WHERE JSON_CONTAINS(tags, JSON_ARRAY('dp'), '$');
   ```
   ✅ 可以走 functional index (MySQL 8.0.17+); ✅ 不需要应用层解析; ❌ 只能查 "包含" 这种简单条件。

3. **`JSON_TABLE` 展开 + 聚合**
   ```sql
   SELECT jt.tag, COUNT(*) cnt
     FROM problems,
   JSON_TABLE(tags, '$[*]' COLUMNS (tag VARCHAR(64) PATH '$')) AS jt
    GROUP BY jt.tag
    ORDER BY cnt DESC;
   ```
   ✅ 一次 SQL 拿统计; ✅ MySQL 8.0+ 原生; ❌ 语法偏复杂, 老版本 MySQL 不支持。

**本项目的 `std::regex` 反序列化有什么问题?**
- 标签如果含转义引号 `\"` 或反斜杠 `\\`, regex `R"("([^"]+)")"` 会匹配错误。
- 标签含中文 / emoji 时, UTF-8 多字节没问题 (regex 默认按字节匹配, 只要不含 `"` 即可), 但性能差。
- 不能校验 JSON 合法性, 数据库里如果有损坏数据, 应用层会抛异常。

**追问方向**: 给 JSON 列建 functional index (`CREATE INDEX idx_tags ON problems((CAST(tags AS CHAR(64) ARRAY));`)? MySQL 8.0.17+ 支持吗?

---

### Q12. `User::removeWithCascade` 手动开事务按 `submission_results → submissions → users` 顺序删除, 而其他外键都用 `ON DELETE CASCADE`。这是为什么? 删除顺序错了会怎样?

**参考答案**

```sql
-- schema.sql:
users (id PK, ...)
  ↑ user_id FK
submissions (id PK, user_id FK → users, ...)
  ↑ submission_id FK
submission_results (id PK, submission_id FK → submissions CASCADE, ...)
```

注意 **`users` 表上没有 `ON DELETE CASCADE`** (虽然 `submissions.user_id` 是 FK, 但 ON DELETE 默认是 RESTRICT)。所以:
- 直接 `DELETE FROM users WHERE id=?` 会失败: 有 submissions 引用。
- 必须先删 submissions, 再删 user。
- submissions 删了之后, submission_results 因为 `ON DELETE CASCADE` 自动跟着删。
- judge_queue 上也有 `submission_id FK CASCADE`, 也会跟着删。

所以顺序必须是 `submission_results (自动) → submissions → users`, 且要包在事务里防止半删状态。

**如果顺序错了**
- `DELETE FROM users` 先 → RESTRICT 报错, 用户删除失败。
- `DELETE FROM submissions` 先 → submission_results / judge_queue 因 CASCADE 自动删, OK。
- 中间任何一步失败, 没有事务就会留下 "孤儿 submissions"。

**追问方向**: schema 里加 `ALTER TABLE submissions ADD CONSTRAINT fk_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE;` 是否更省事? 提示: 业务上 "用户注销" 是软删除还是硬删除? GDPR 要求?

---

## 四、并发与线程安全 (3 题)

### Q13. SessionManager 有专门的并发测试 (`ConcurrentCreateSession` / `ConcurrentValidateSession` / `ConcurrentMixedOperations`), SessionManager 在多线程下可能出什么问题? 现有 `unordered_map + mutex` 方案在高并发下瓶颈?

**参考答案**

**潜在问题**
- **死锁**: 如果某方法内部调用另一需要加锁的方法, 又没排序锁顺序, 可能自死锁; 当前实现 `create` / `validate` / `remove` 都是直接 `std::lock_guard`, 没有重入。
- **迭代器失效**: 删除 / 插入时若同时有人遍历 map (`for (auto& [k,v] : sessions_)`), UB; 本项目 `MixedOperations` 测试没专门覆盖这个。
- **TTL 清理线程**: 当前实现没有后台 TTL 清理, 24h 过期不会主动释放, 长期会内存泄漏。
- **Token 碰撞**: 63 bit 随机数, 生日碰撞概率: 2^31 个 session 时约 50% (实际不可能, 但理论上 `std::uniform_int_distribution<long long>(mt19937_64)` 不会重复除非 RNG 重启)。

**性能瓶颈**
- `std::mutex` 是排他锁: 1000 并发 validate 时串行化, 单核 CPU 等价 1 路处理。
- 替代方案:
  1. **读写锁 `std::shared_mutex`**: validate 是读多写少, 加共享锁允许并发读; create / remove 用独占锁。QPS 可提升 5~10 倍。
  2. **分片 (sharded map)**: 16 个 `unordered_map` + 16 把锁, 按 token 哈希分片, 减少锁粒度。
  3. **外部存储**: MySQL / Redis, 多实例共享。

**追问方向**: 如何写一个测试, 故意让 1000 个线程同时调用 create / validate / remove, 验证 map 不损坏? 用 TSAN (ThreadSanitizer) 检测数据竞争。

---

### Q14. `judge_worker` 默认 4 线程 + `SELECT FOR UPDATE` 抢占任务, 多 worker 进程部署时是否会产生"批处理分裂"? worker 突然挂掉, 那些 `status='running'` 的任务会怎样?

**参考答案**

**批处理分裂 (batch split)**: 不存在, 因为每个任务独立抢占, 不需要 batch。但有相关问题:
- **优先级反转**: 高优先级任务如果被低优先级 worker 先抢走, 高优先级要等下一次轮询; 在没有专用优先级队列时不可避免。
- **CPU 浪费**: 一旦抢到任务, 该 worker 线程会一直处理这个任务 (编译 + N 个 fork), 期间其他 worker 抢新任务; 4 worker 并发跑 4 个 "while(1) { fork(); }" 攻击代码, 每个进程跑满 cgroup CPU 配额, 其他用户的提交全卡住。

**worker 突然挂掉 (`SIGKILL` / OOM / 机器宕机)**
- 任务卡在 `status='running'`, 没有 reclaim 逻辑。
- 重启后 worker 不会去扫描 `status='running'` 行 (除非显式加 startup reclaim)。
- 用户体验: 提交一直停在 "Judging...", 永远看不到结果。
- 配置里有 `worker.max_retries=3`, 但代码里 grep 不到相关 retry 实现 (待验证)。

**修复建议**
1. 启动时扫描: `UPDATE judge_queue SET status='pending', worker_id=NULL WHERE status='running' AND started_at < NOW() - INTERVAL 5 MINUTE;`
2. 每次 pop 时也跑一次 reclaim (额外一次 UPDATE, 但能兜底)。
3. 心跳表 `worker_heartbeat(worker_id, last_heartbeat)`, worker 每 10s 写一次, 主进程 / admin 端能报警。

**追问方向**: 用 `SELECT ... FOR UPDATE SKIP LOCKED` (MySQL 8.0+) 能否完全消除竞争? 设计一个 leader election 让只有一个 worker 做 reclaim。

---

### Q15. `ConnectionPool::getConnection` 每次调用都 `mysql_ping` 一下, 为什么? 会有什么副作用?

**参考答案**

```cpp
// src/db_pool/connection_pool.cc (大致)
MYSQL* conn = /* from pool */;
if (mysql_ping(conn) != 0) {
    // 连接死了, 关闭并重建
    mysql_close(conn); conn = mysql_init(...); mysql_real_connect(...);
}
```

**为什么 ping**:
- MySQL 默认 `wait_timeout=28800s` (8 小时), 闲置连接会被服务端主动断开。
- 网络抖动、MySQL 重启、容器迁移 都会让已有连接失效。
- 不 ping 的后果: 用户请求过来拿到一个已死的连接, `mysql_real_query` 报 `CR_SERVER_GONE_ERROR`, handler 500。

**副作用 / 代价**
- `mysql_ping` 一次 round-trip (本地 MySQL 约 0.1~0.3ms, 跨机房可能 1~5ms), 每次取连接都付这个成本。
- 高并发下 ping 本身又抢 MySQL 连接, 反而成为瓶颈。
- 本项目连接池默认 10, 100 QPS 时每秒 100 次 ping, 不可忽略。

**改进方案**
1. **惰性 ping**: 只在 `mysql_real_query` 返回错误码 2006/2013 (CR_SERVER_GONE / CR_SERVER_LOST) 时才重建; 平时不 ping。
2. **keepalive 设置**: `mysql_options(&conn, MYSQL_OPT_RECONNECT, &reconnect)` + `MYSQL_OPT_CONNECT_TIMEOUT`; 或者 OS 层 TCP keepalive。
3. **定期 ping**: 后台线程每 60s ping 一次池中所有连接, 不阻塞 getConnection 路径。

**追问方向**: `MYSQL_OPT_RECONNECT` 在 MySQL 5.x 和 8.0 行为有什么变化? (提示: 8.0 默认 reconnect 行为改变。)

---

## 五、安全与沙箱 (3 题)

### Q16. 为什么用 cgroup + `fork` 而不是 Linux namespace + seccomp-bpf? 给一段学生代码 `while(1){fork();}` 会发生什么? cgroup v1/v2 文件名错配 (`cpu.max` vs `cpu.cfs_quota_us`) 的后果?

**参考答案**

**为什么 cgroup 而非 namespace**
- ✅ cgroup 限制的是 **资源** (CPU / 内存 / 进程数), 不能做权限隔离。
- ❌ **本项目没有 user namespace / mount namespace / network namespace**, 也没有 seccomp 白名单。
- 学生代码以 root (或当前用户) 运行, 可以:
  - 读宿主机任意文件 (包括 `/etc/shadow`、DB 密码)。
  - 攻击内网 (连出 Redis, SSRF)。
  - 发网络包攻击 MySQL 同机其他用户。
  - 改系统配置 (写 `/etc/...`)。

**`while(1){fork();}` 攻击路径**
1. fork 进程数激增。
2. cgroup v1 `pids.max` 或 v2 `pids.max` 应该限制; **如果 cgroup 没正确 mount, `pids.max` 写入失败**, 进程数爆炸。
3. 即使 cgroup 生效, 父进程 (`judge_worker` 的 worker 线程) `waitpid` 收集 SIGCHLD, 子进程先耗尽 cgroup, 父进程继续走 polling, 不会直接挂。
4. 但是: 学生进程在 cgroup 里耗尽内存 → OOM kill (cgroup 内 OOM), 或者由父进程读到 memory.peak 超限 → SIGKILL; 最终都被父进程清理掉。**只要 cgroup 正确 mount, 这段代码基本可控**。

**cgroup v1 vs v2 文件名错配 (本项目已知 bug)**
- 代码 (`cgroup_manager.cpp`) 写的是 v2 文件名: `cpu.max`, `memory.max`, `pids.max`, `cgroup.procs`.
- README / DEPLOY / spec.md 声称是 v1: `cpu.cfs_quota_us`, `memory.limit_in_bytes`, `tasks`.
- **Ubuntu 22.04 默认 hybrid 模式, 两个版本共存**, v2 控制器位于 `/sys/fs/cgroup/`, 因此代码刚好能跑。
- **Ubuntu 20.04 默认 v1**, v2 文件名不存在 → `open()` 返回 ENOENT → cgroup 隔离失效, 退回 `setrlimit` 兜底。
- **CentOS / RHEL 默认 v1**, 隔离直接失败。

**修复建议**
1. 启动时检测 `/sys/fs/cgroup/cgroup.controllers` 存在与否, 自动选择 v1/v2。
2. 把限制逻辑做成两套, 按 kernel 版本切换。
3. 真正的安全方案: user namespace + seccomp (`--seccomp` flag + 白名单 `read/write/exit_group/...`).

**追问方向**: 用 Firecracker / gVisor 替代 cgroup, 性能开销 / 安全边界 各是怎样?

---

### Q17. Session token 是 `std::to_string(uniform_int_distribution<long long>(mt19937_64))`, 只有约 63 bit 熵且未编码; cookie 没有 `Secure` / `SameSite`。生产环境会怎样? 改进方案?

**参考答案**

**Token 安全性分析**
- `uniform_int_distribution<long long>(mt19937_64)` 在 64 bit 平台上 `long long` 是 64 bit, 但实际均匀范围是 `[-2^63, 2^63-1]`; 熵 ≈ 63 bit (因为正负半区间各一半, 但输出范围实际是 `[-2^63, 2^63-1]`).
- `std::to_string(int64)` 输出最长 19 位十进制数字, 没有前导 0; 攻击者拿到一个合法 token 后大致知道它是 19 位数字, 可搜索空间比 128 bit UUID 少得多。
- 没有过期轮转 / 绑定 IP / 绑定 User-Agent; 拿到 token = 完全劫持会话。

**Cookie 标志缺失**
- 没有 `Secure`: 允许 HTTP 明文传输, 中间人可窃取。
- 没有 `SameSite`: CSRF 风险 (虽然有 `HttpOnly` 防 XSS 读, 但不能防 CSRF)。
- 没有 `__Host-` / `__Secure-` 前缀: 现代浏览器不能强制附加安全策略。

**生产改进**
```cpp
// 1. 256 bit 熵, base64url 编码
std::array<uint8_t, 32> buf;
RAND_bytes(buf.data(), buf.size());                    // OpenSSL CSPRNG
std::string token = base64url_encode(buf);             // 43 字符

// 2. 绑定到用户
session.user_id = uid;
session.created_at = now();
session.expires_at = now() + 1h;
session.fingerprint = sha256(user_agent || ip);        // 校验时再哈希比对

// 3. Cookie 标志
Set-Cookie: session_token=...; HttpOnly; Secure; SameSite=Strict; Path=/; Max-Age=3600

// 4. 滑动过期 + 撤销
// 5. 异常登录检测 (异地 / 异常 UA / 高频次请求)
```

**追问方向**: JWT (HS256) vs Session + Redis, 在 "可撤销" "可水平扩展" "性能" 三个维度各打几分?

---

### Q18. `escapeString` 只转义单引号, 当用户提交的 `code` 字段含反斜杠或 NUL 字节时会怎样? 完整的 SQL 注入修复路径?

**参考答案**

**当前实现的"安全幻觉"**
- `escapeString(s)`: 遍历字符串, `if (c == '\'') out += "\\'"; else out += c;`
- 不处理: `\\` (反斜杠)、`"` (双引号, 在 ANSI_QUOTES 模式下是字符串界定符)、NUL `\0`、`%` / `_` (LIKE 通配符)。
- 在 MySQL 默认非 ANSI_QUOTES 模式下, 单引号是唯一字符串界定符, 所以 `code` 这种 TEXT 字段 (用户控制, 但拼接模板里只有一个 `'%s'` 占位) **事实上不会发生 SQL 注入**, 因为:
  - `code` 里的反斜杠不闭合任何结构;
  - `code` 里的单引号被 `escapeString` 转义;
  - `code` 里的分号 `;` 是字符数据的一部分。
- **但有别的 bug**:
  - 用户输入 `"admin' OR 1=1 --"` 写到 `username` 字段, `escapeString` 把单引号转义, 安全。
  - 用户输入 `"100% match"`, 写入没问题; 但如果代码里对该字段做 `LIKE '%user_input%'`, 通配符未转义, 可能匹配错。
  - 用户输入含 NUL 字节: C 字符串截断, 之后的字符丢; 但二进制安全由 `mysql_real_query` 接管 (它接受 `length` 参数), 所以长度仍 OK, 只是日志里看不到尾巴。

**真正的修复路径**
1. **统一迁移到预处理语句**: `MYSQL_STMT* stmt = mysql_stmt_init(conn); mysql_stmt_prepare(stmt, sql, len); MYSQL_BIND bind[N]; mysql_stmt_bind_param(stmt, bind); mysql_stmt_execute(stmt);`
2. **如果坚持字符串拼接, 用 `mysql_real_escape_string`**: 它处理 `'`、`"`、`\\`、NUL、`\n`、`\r`、`Ctrl-Z`, 比手写函数可靠得多; 调用前必须 `mysql_set_character_set(conn, "utf8mb4")`.
3. **LIKE 查询单独写**: 用户输入含 `%` / `_` 时手动 `\` 转义 (MySQL 默认 LIKE 转义符是 `\\`).
4. **代码层加单元测试**: 注入 `'\\` `"; DROP TABLE users; --` `%` `_` UTF-8 多字节, 验证 SQL 结构不被破坏。

**追问方向**: 现在迁移到 prepared statements, 哪些 model 影响最大? 大概多少行 SQL 要改?

---

## 六、算法与业务逻辑 (2 题)

### Q19. `JudgeService::compareOutput` 用 "第一遍 trimmed 严格 + 第二遍空白容忍" 区分 AC / WA / PE, 描述算法、边界条件和 corner case。

**参考答案**

**算法** (来自 `judge_service.cpp` lines 381-449)

```cpp
void compareOutput(const std::string& actual, const std::string& expected,
                   bool*is_ac, bool*is_wa, bool*is_pe);
```

- **第一遍**: 把两段输出按行 split, 每行做 `trim()` (去首尾空白), 严格相等则 `*is_ac = true` 返回。
- **第二遍**: 同样 split, 但每行做 `normalize()` (把连续空白压缩成单个空格), 比较; 若全部相等则 `*is_pe = true` (PE)。
- **都失败** 则 `*is_wa = true`.

**Corner cases**
1. **末尾空行**: 学生输出 `1\n2\n3\n`, 期望 `1\n2\n3`, 严格比较相等 → AC; 但学生输出 `1\n2\n3\n\n\n`, 期望 `1\n2\n3`, split 后多两个空行, 严格比较失败, 进入第二遍; normalize 后还是多两空行 → WA (而不是 PE). **这是争议点**: 多数 OJ 把 "末尾多空行" 当 PE。
2. **CRLF vs LF**: Windows 风格 `\r\n` 与 Linux `\n`, 严格比较失败; 第二遍 normalize 不处理 `\r`, 仍 WA. 应当 normalize 时把 `\r\n` → `\n`.
3. **行尾空格**: 学生 `1 ` 与期望 `1`, 第一遍 trim 后相等 → AC; 但有些 OI 题目明确禁止行尾空格, 此时应 WA / PE.
4. **大小写**: `Yes` vs `yes`, 严格失败, normalize 不处理 → WA; 多数 OJ 默认大小写敏感, 这是预期。
5. **浮点数**: `1.0` vs `1.00`, 严格失败, normalize 不处理 → WA; 通常 OJ 需要 `special judge` 或允许 `1e-6` 误差。
6. **空格 vs Tab**: 第一遍 trim 后都成空, normalize 把多空白压成单空格, 所以 Tab 与空格等价 → AC. 部分 OJ 严格区分。
7. **空输出**: 期望空 `""`, 学生也输出空, 第一遍 split 出来 0 行, 第二遍也 0 行 → AC. 期望空, 学生输出 `"\n"`, split 出 1 个空字符串 → WA / PE (边界).

**改进方向**
- 末尾多空行视为 PE。
- normalize 时去除 `\r`。
- 提供题目级 flag: `strict_whitespace`, `ignore_case`, `float_tolerance`.

**追问方向**: 写一段 5 行的单测覆盖上述 7 个 corner case。

---

### Q20. Verdict 聚合用 TLE > MLE > RE > WA > PE > AC 的优先级, 含义是 "取最严重的", 与许多 OJ "取第一个失败的" 策略相反。讨论这两种策略对用户调试体验的差别。

**参考答案**

**本项目策略 (最严重)**

```cpp
// judge_service.cpp lines 243-255 (大致)
Verdict worst = AC;
for (auto& r : results) {
    worst = std::max(worst, r.status);  // 按枚举优先级
}
return worst;
```

例: 10 个测试用例, 第 1 个 WA, 第 5 个 TLE, 第 10 个 AC → 整体 TLE.

用户体验: 看到 TLE, 但不知道 WA 也存在; 可能调了半天算法复杂度, AC 了才发现 WA 还在。

**许多 OJ 的策略 (第一个失败)**

例: 第 1 个 WA → 整体 WA, 后面测试直接 skip.

用户体验: 立刻知道第一个失败的原因, 但看不到更严重的错误 (例如第 1 个 WA 实际是 PE, 第 5 个 TLE, 你只看 WA 修了 PE, 才发现 TLE 更要命).

**对比**

| 策略 | 用户体验 | 系统资源 | 适用场景 |
|---|---|---|---|
| 最严重 | 一次性看到所有严重错误, 但要自己比对多组结果 | 跑完所有测试 | 教学 OJ (鼓励学生看全部结果) |
| 第一个失败 | 快速定位, 但可能漏看严重错误 | 提前终止 | 比赛 OJ (限时, 反馈快) |

**本项目作为教学 OJ, 选 "最严重" 是合适的**; 但应该在 UI 上把 **所有** 测试用例的 verdict 都列出来 (`submissions` 页面已经有这个表格), 避免学生漏看。

**追问方向**: 如果同时支持两种策略, 应该在哪里配置? 提示: `problems.verdict_strategy` 字段 + admin 后台 toggle。

---

## 附: 项目速查表 (面试开场可问)

| 项 | 实际情况 |
|---|---|
| 语言 / 标准 | C++20, gcc |
| HTTP 框架 | cpp-httplib (vendored 单头, ~20K 行) |
| DB | MySQL 5.7+/8.0, libmysqlclient, **无 ORM** |
| 队列 | MySQL `judge_queue` 表 + `SELECT FOR UPDATE` |
| 沙箱 | cgroup v1/v2 + setrlimit, **无 namespace / seccomp** |
| 密码 | OpenSSL `crypt_gensalt_rn` (bcrypt cost=10) |
| 进程模型 | `oj_server` + `judge_worker` 双进程 |
| 测试 | GoogleTest (C++ 单元/集成) + pytest (Python e2e) |
| 构建 | CMake 3.10+, `find_package(GTest)` |
| 前端 | 原生 HTML/CSS/JS + CodeMirror 5 (CDN) |
| 文档 | README.md / DEPLOY.md / API.md / spec.md / RESUME.md |