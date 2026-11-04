# 聊天业务拆分

请求链路：`Connection → ChatServer → Router → ChatHandlers → MessageStore`。

| 模块 | 职责 |
| --- | --- |
| Connection | TCP 收发、缓冲、事件监听和关闭 |
| ChatServer | 连接回调、HTTP 增量解析、响应发送和解析器清理 |
| ChatHandlers | 路由注册、页面、查询参数、HTTP 状态和 JSON 输出 |
| MessageStore | 消息 ID、内存历史保留、按 ID 查询 |

`main` 创建 `MessageStore`，再通过引用传入 `ChatServer`。存储必须比服务器活得久；服务器也必须比它管理的连接回调活得久。`ChatHandlers` 的路由捕获自身地址，因此注册之后不能移动它或使它先于 Router 析构。ChatServer 禁止复制。

MessageStore 不依赖 HTTP、socket 或事件循环。它默认保留最近 1000 条消息，使用 deque 淘汰最旧消息，查询返回独立快照；复制期间持锁，JSON 序列化在锁外执行。只有存储支持并发调用，连接、解析器和 Router 的注册仍由事件循环线程管理。

现有接口保持不变：`GET /`、`POST /send`（空消息忽略，返回 302）、`GET /messages?since=<id>`（返回 id 严格大于 since 的保留消息）。存储仍在内存中，重启会清空；当前容量只限制条数，不限制总字节数。JSON 输出补齐了 ASCII 控制字符转义。

后续增量：

1. 房间：在 Message 和存储操作中增加 room_id，HTTP 层负责从请求提取房间。
2. 分页：给历史查询增加 limit 和游标，并明确历史已淘汰时的返回语义。
3. 持久化：业务规则增多时再拆出 ChatService 和存储接口；数据库 I/O 不应阻塞事件循环。
4. 推送：在服务层增加事件发布，独立处理订阅、慢消费者和断线续传。

编译与测试（在项目根目录执行）：

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -I. main.cpp connection.cpp event_loop.cpp socket_utils.cpp -o /tmp/chat_server
g++ -std=c++20 -Wall -Wextra -Wpedantic -pthread -I. test/chat_business_test.cpp -o /tmp/chat_business_test
/tmp/chat_business_test
```

业务测试覆盖历史淘汰、ID、快照、并发存取、HTTP 路由及 JSON 控制字符转义。原有 http_parser_test.cpp 的 Content-Length=18 与期望正文 20 字节不一致，需单独修正后才能作为全量测试使用。
