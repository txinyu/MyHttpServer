# MyHttpServer - 基于 muduo 的 HTTP/1.1 服务器

MyHttpServer 是一个基于 muduo 网络库实现的 C++ HTTP/1.1 服务器项目，采用 Reactor 事件驱动模型，围绕 HTTP 请求解析、路由分发、中间件扩展、静态文件服务、Keep-Alive 长连接、异步线程池和性能压测等模块进行设计与实现。

项目主要用于学习和实践 C++ 后端开发、Linux 网络编程、muduo 网络库、HTTP 协议解析和高并发服务器设计。整体代码采用模块化设计，便于阅读、扩展和面试展示。

---

## 项目背景

muduo 自带的 HTTP 示例模块功能较为基础，主要用于展示网络库能力，缺少完整的路由系统、中间件机制、静态文件处理、路径参数、缓存控制、Range 请求等常见 Web 服务能力。

基于此，本项目在 muduo 的 TCP 网络层之上重新设计 HTTP 服务框架，实现了从 TCP 字节流解析 HTTP 请求、路由匹配、业务处理到 HTTP 响应序列化发送的完整链路，并通过自研压测工具对 Keep-Alive 长连接场景下的吞吐和延迟进行验证。

---

## 核心特性

- **HTTP/1.1 请求解析**  
  基于状态机实现请求行、请求头、请求体解析，支持 `Content-Length`、Chunked 编码、Keep-Alive 长连接等 HTTP/1.1 常见机制。

- **Keep-Alive 长连接支持**  
  支持 HTTP/1.1 默认长连接，根据请求头和响应状态决定是否保持连接，避免每次请求都重新建立 TCP 连接。

- **Radix Tree 路由系统**  
  使用 Radix Tree 实现路由匹配，支持静态路由、路径参数路由，例如 `/user/:id`，以及通配符路由。

- **中间件机制**  
  基于责任链模式设计中间件体系，可扩展日志、鉴权、跨域、限流、耗时统计等通用逻辑。

- **静态文件服务**  
  支持静态资源访问、MIME 类型识别、路径安全检查、防目录穿越、目录索引、HTTP 缓存控制、ETag、Last-Modified 和 Range 范围请求。

- **异步线程池**  
  支持将耗时业务逻辑提交到线程池中处理，降低 IO 线程被阻塞的风险。

- **自研压测工具**  
  提供短连接压测、Keep-Alive 长连接压测和单请求延迟统计工具，可输出 QPS、平均延迟、P50、P90、P99 等指标。

- **工程化构建**  
  使用 CMake 管理构建，提供 `build.sh` 一键编译脚本，支持服务器和多个 bench 工具统一编译。

---

## 性能测试

### 测试环境

当前测试基于本机 loopback 环境完成：

- 操作系统：Linux / WSL Linux 环境
- 编译标准：C++17
- 网络库：muduo
- 构建工具：CMake 3.10+
- 测试接口：`GET /hello`
- 响应内容：`Hello World!`
- 响应大小：12 Bytes
- 连接模式：HTTP Keep-Alive 长连接

### Keep-Alive 单请求延迟压测

使用 `bench/latency_bench.cpp` 对 `/hello` 接口进行压测。

测试方式：

- 16 个线程
- 每个线程维护 1 条 Keep-Alive 长连接
- 总请求数 100000
- 每次请求从 `send()` 开始计时，到完整 HTTP 响应接收完成结束
- 根据 `Content-Length` 判断响应是否完整

测试结果示例：

| 指标 | 结果 |
|---|---:|
| 总请求数 | 100000 |
| 成功请求数 | 100000 |
| 失败请求数 | 0 |
| QPS | 约 4.46 万 |
| 平均延迟 | 约 355 μs |
| P50 延迟 | 约 326 μs |
| P90 延迟 | 约 526 μs |
| P99 延迟 | 约 1.01 ms |

说明：以上数据为本机 loopback 环境下的测试结果，主要用于验证服务器 Keep-Alive 连接复用、HTTP 响应完整性和请求处理链路的稳定性。不同硬件、系统负载、编译参数和并发连接数下结果会有所差异。

---

## 项目结构

```text
MyHttpServer/
├── bench/                       # 压测程序目录
│   ├── bench.cpp                # 短连接压测程序
│   ├── bench_keepalive.cpp      # Keep-Alive 长连接吞吐压测程序
│   └── latency_bench.cpp        # 单请求延迟压测程序，统计平均延迟、P50/P90/P99
├── src/                         # 核心源码目录
│   ├── utils.h/cc               # 工具函数：状态码、MIME、URL 编解码、文件安全等
│   ├── http_request.h/cc        # HTTP 请求对象：方法、路径、参数、请求头、请求体
│   ├── http_response.h/cc       # HTTP 响应对象：状态码、响应头、响应体、文件响应
│   ├── http_context.h/cc        # HTTP 解析上下文：请求行、请求头、请求体状态机解析
│   ├── radix_tree.h/cc          # Radix Tree 路由实现：静态/参数/通配符路由
│   ├── middleware.h/cc          # 中间件责任链
│   ├── static_file_handler.h/cc # 静态文件处理：缓存、Range、路径安全检查
│   ├── thread_pool.h            # 异步线程池
│   └── http_server.h/cc         # HTTP 服务器主类，整合解析、路由、响应发送等模块
├── www/                         # 静态资源目录
├── main.cc                      # 服务器启动示例
├── build.sh                     # 一键编译脚本
├── CMakeLists.txt               # CMake 构建配置
├── README.md                    # 项目说明文档
├── .gitignore                   # Git 忽略文件
└── LICENSE                      # 开源许可证
```
---
## 环境依赖

- 操作系统：Linux，推荐 Ubuntu 20.04 / 22.04
- 编译器：支持 C++17 的 GCC/G++
- 构建工具：CMake 3.10+、make
- 第三方库：
  - muduo
  - Boost system
  - Boost thread
  - pthread

---

## 快速编译与启动

### 1. 克隆项目

```bash
git clone https://github.com/txinyu/MyHttpServer.git
cd MyHttpServer
```

### 2. 一键编译

```bash
chmod +x build.sh
./build.sh
```

编译完成后，可执行文件会生成在：

```text
build/bin/
```

包括：

```text
build/bin/my_http
build/bin/bench
build/bin/bench_keepalive
build/bin/latency_bench
```

---

## 运行服务器

启动 HTTP 服务器：

```bash
./build/bin/my_http
```

默认监听端口为：

```text
8080
```

测试 `/hello` 接口：

```bash
curl -i http://127.0.0.1:8080/hello
```

正常响应示例：

```http
HTTP/1.1 200 OK
Connection: keep-alive
Content-Type: text/plain
Content-Length: 12

Hello World!
```

---

## 运行压测

### Keep-Alive 单请求延迟压测

先启动服务器：

```bash
./build/bin/my_http
```

另开一个终端执行：

```bash
./build/bin/latency_bench
```

输出示例：

```text
===== 长连接单请求延迟压测结果 =====
线程数: 16
目标请求数: 100000
成功请求数: 100000
失败请求数: 0
总耗时: 2.24177 s
QPS: 44607

===== 单请求延迟统计 =====
平均延迟: 355.416 us
P50 延迟: 326.417 us
P90 延迟: 526.434 us
P99 延迟: 1013.67 us
最小延迟: 56.432 us
最大延迟: 3681.36 us
```

### 其他压测工具

短连接压测：

```bash
./build/bin/bench
```

Keep-Alive 吞吐压测：

```bash
./build/bin/bench_keepalive
```

---

## 核心模块说明

### 1. HttpServer

`HttpServer` 是服务器主控模块，负责整合 muduo 的 `TcpServer`、HTTP 解析器、路由系统、中间件、静态文件处理和响应发送逻辑。

主要职责：

- 监听 TCP 连接
- 为每条连接绑定 `HttpContext`
- 在 `onMessage()` 中解析 HTTP 请求
- 请求解析完成后调用路由或静态文件处理模块
- 统一发送 HTTP 响应
- 根据 Keep-Alive 判断是否关闭连接

---

### 2. HttpContext

`HttpContext` 是 HTTP 请求解析状态机，负责从 muduo `Buffer` 中逐步解析 HTTP 请求。

解析状态包括：

```text
请求行
请求头
请求体
解析完成
解析错误
```

该模块能够处理 TCP 字节流场景下的半包、粘包问题。当数据不足时保留当前解析状态，等待下一次 `onMessage()` 继续解析。

---

### 3. HttpRequest

`HttpRequest` 是 HTTP 请求对象，保存请求方法、路径、协议版本、请求头、查询参数、路径参数和请求体。

主要能力：

- 获取请求头
- 获取 query 参数
- 获取路径参数
- 判断是否 Keep-Alive
- 获取 Content-Length

---

### 4. HttpResponse

`HttpResponse` 是 HTTP 响应对象，负责保存响应状态码、响应头、响应体和文件响应信息。

主要能力：

- 设置状态码
- 设置响应内容
- 设置响应头
- 设置文件响应
- 序列化为 HTTP/1.1 响应报文
- 根据 `close_connection` 控制响应后是否关闭连接

---

### 5. RadixTree

`RadixTree` 是路由匹配模块，按照 HTTP Method 维护路由树。

支持：

- 静态路由：`/hello`
- 参数路由：`/user/:id`
- 通配符路由：`/static/*filepath`

匹配成功后返回对应 handler，并提取路径参数。

---

### 6. Middleware

中间件模块基于责任链模式实现。每个中间件可以选择继续执行 `next()`，也可以提前终止请求处理流程并直接返回响应。

适合扩展：

- 日志记录
- 鉴权
- 跨域
- 限流
- 请求耗时统计

---

### 7. StaticFileHandler

静态文件处理模块用于将 URL 路径映射到本地文件系统路径。

支持：

- MIME 类型识别
- 路径安全检查
- 防目录穿越
- 默认目录首页
- 目录列表
- ETag
- Last-Modified
- Cache-Control
- Range 范围请求

当前文件发送采用分块读取并发送的方式，避免一次性将大文件完整读入内存。

---

### 8. ThreadPool

线程池模块用于处理耗时业务任务，避免复杂业务逻辑阻塞 IO 线程。

基本流程：

```text
IO 线程解析请求
    ↓
业务任务提交到线程池
    ↓
工作线程执行业务逻辑
    ↓
处理完成后回到 EventLoop 发送响应
```

---

## 示例用法

`main.cc` 中可以注册普通路由、参数路由和静态文件服务。

示例：

```cpp
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include "src/http_server.h"

int main() {
    muduo::net::EventLoop loop;
    muduo::net::InetAddress addr(8080);

    HttpServer server(&loop, addr, "MyHttpServer");

    server.setIoThreads(4);

    server.get("/hello", [](const HttpRequest& req, HttpResponse* resp) {
        resp->set_content("Hello World!", "text/plain");
    });

    server.get("/user/:id", [](const HttpRequest& req, HttpResponse* resp) {
        std::string id = req.get_path_param("id");
        resp->set_content("User ID: " + id, "text/plain");
    });

    server.serveStatic("/static", "./www");

    server.start();
    loop.loop();

    return 0;
}
```

---

## 优化与实现亮点

### 状态机解析 HTTP 请求

使用 `HttpContext` 保存解析状态，适配 TCP 流式数据，能够处理数据分包和粘包场景。

### Keep-Alive 连接复用

修复响应连接控制逻辑，根据请求头和响应状态正确返回 `Connection: keep-alive` 或 `Connection: close`。

### Radix Tree 路由匹配

支持静态路由、参数路由和通配符路由，便于构建 RESTful 风格接口。

### 中间件责任链

将日志、鉴权、限流等通用逻辑从业务 handler 中解耦出来。

### 静态文件处理

支持 MIME、缓存、Range、路径安全检查和目录索引等功能。

### 自研压测工具

不仅统计 QPS，还统计从请求发送到完整 HTTP 响应接收完成的单请求延迟，并输出 P50/P90/P99。

---

## 已知改进方向

- 完善请求体分包场景下的 body 追加逻辑。
- 静态文件发送可进一步优化为 `sendfile` 或 `mmap` 方式。
- 异步任务回调可进一步绑定到连接所属的 EventLoop。
- 增加连接超时管理，支持空闲连接自动释放。
- 增加更完整的异常日志和错误码统计。
- 增加 wrk / ab 等第三方工具压测结果作为对照。
- 增加单元测试和 CI 构建流程。

---

## 相关文档

项目详细优化原理、源码解析与实现细节，可参考技术博客：

[muduo http 优化 —— 在原本数据监测 HTTP 上支持功能完善的 HTTP/1.1](https://blog.csdn.net/m0_62807361/article/details/159774170?spm=1001.2014.3001.5501)

---

## 开源许可证

本项目采用 MIT License 开源，允许自由使用、修改和分发。