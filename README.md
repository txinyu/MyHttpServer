# MyHttpServer - 基于muduo的高性能HTTP/1.1服务器

基于muduo网络库深度优化开发，在原生muduo数据监测HTTP基础上，扩展实现了功能完整、高性能、可扩展的HTTP/1.1服务器，遵循Reactor事件驱动模型，采用分层架构与模块化设计，适配高并发Web服务场景，代码结构清晰、可维护性强，完全符合工程化开发规范。

## 📖 项目背景

muduo原生HTTP模块设计目标为数据监测，存在架构耦合严重、路由匹配低效、功能扩展困难等问题，难以应对高并发请求场景；同时传统简单HTTP服务器普遍存在报文解析不稳定、静态资源访问效率低等痛点，无法满足实际业务中低延迟、可扩展的需求。基于此，本项目从零设计实现了一套通用工业级HTTP服务器，解决上述痛点，提升性能与可扩展性。

## ✨ 核心特性

- 完整HTTP/1.1协议支持：实现请求行、请求头、请求体的高效解析，支持chunked编码、长连接（Keep-Alive），兼容HTTP/1.1标准规范

- 高性能路由系统：采用Radix Tree（基数树/前缀树）实现路由匹配，支持静态路由、路径参数（如/user/:id）、通配符路由（如/static/*filepath），匹配效率O(k)（k为路径段数）

- 中间件扩展机制：基于责任链模式设计中间件体系，支持日志、鉴权、跨域、限流等通用功能插件化接入，灵活扩展业务逻辑

- 静态文件服务：内置静态文件处理模块，支持sendfile零拷贝传输、MIME类型自动识别、路径安全检查（防目录穿越），提升静态资源访问效率

- 高并发优化：IO线程与业务线程分离，搭配异步线程池，支持连接复用，长连接场景下压测性能优异

- 完善的工具支撑：提供URL编解码、HTTP状态码映射、文件操作、字符串处理等通用工具，降低开发成本

- 工程化规范：基于C++17开发，采用CMake构建，代码模块化、低耦合、高内聚，可直接用于生产或面试展示

## 📊 性能测试

### 测试环境

Linux环境 + muduo网络库 + C++17 + CMake 3.10+，编译开启-O2优化

### 测试结果

- TCP短连接压测：QPS约1万（符合常规短连接性能预期）

- Keep-Alive长连接压测：QPS可达23万+（连接复用大幅提升吞吐量）

### 性能对比（与主流开源HTTP服务器）

本项目在长连接场景下性能表现优异，远超传统脚本语言框架，接近甚至超越Nginx等专业Web服务器，核心优势在于Radix树路由、零拷贝传输与线程池优化。

## 📂 项目结构
 ``` 
MyHttpServer/
├── bench/                  # 压测程序目录
│   ├── bench.cpp           # 短连接压测程序
│   └── bench_keepalive.cpp # 长连接压测程序
├── src/                    # 核心源码目录
│   ├── utils.h/cc          # 通用工具：状态码、MIME、URL编解码、文件安全等
│   ├── http_request.h/cc   # HTTP请求封装类（方法、路径、参数、请求头）
│   ├── http_response.h/cc  # HTTP响应封装类（状态码、响应头、响应体、零拷贝）
│   ├── http_context.h/cc   # HTTP报文解析状态机（驱动请求解析流程）
│   ├── radix_tree.h/cc     # Radix树路由实现（静态/参数/通配符路由）
│   ├── middleware.h/cc     # 中间件责任链（扩展通用业务逻辑）
│   ├── static_file_handler.h/cc # 静态文件处理（零拷贝、缓存、安全检查）
│   ├── thread_pool.h       # 异步线程池（解耦耗时操作，提升并发）
│   ├── http_server.h/cc    # 服务器主类（整合所有模块，提供对外接口）
│   └── main.cc             # 服务器启动示例（路由注册、服务启动）
├── www/                    # 静态资源目录（存放HTML、CSS、图片等）
├── build.sh                # 一键编译脚本（清理、构建、编译）
├── CMakeLists.txt          # 工程构建配置（编译优化、依赖链接）
├── README.md               # 项目说明文档（本文档）
├── .gitignore              # Git 忽略文件
└── LICENSE                 # 开源许可证
 ``` 

## 🔧 环境依赖

- 操作系统：Linux（推荐Ubuntu 22.04）

- 编程语言：C++17（需支持C++17标准的编译器，如gcc/g++ 9+）

- 依赖库：muduo网络库、Boost（system、thread组件）

- 构建工具：CMake 3.10+、make

## 🚀 快速编译与启动

### 编译步骤

 ```
# 克隆项目（本地已有可跳过）
git clone <你的项目仓库地址>
cd MyHttpServer

# 一键编译（脚本会自动清理旧构建、生成Makefile、多线程编译）
./build.sh
 ```

### 编译产物

编译完成后，可执行文件将生成在 build/bin/ 目录下：

- build/bin/my_http：HTTP服务器主程序

- build/bin/bench：短连接压测工具

- build/bin/bench_keepalive：长连接压测工具

### 启动服务器

 ```
# 进入构建产物目录
cd build/bin

# 启动HTTP服务器（默认监听80端口，可在main.cc中修改）
./my_http
 ```

启动成功后，浏览器访问 http://localhost 即可查看静态资源首页。

## 🔍 核心模块说明

1. 基础支撑层：utils模块提供通用工具，thread_pool提供异步线程池，为上层模块提供基础能力

2. 协议解析层：HttpRequest/HttpResponse封装请求与响应报文，HttpContext通过状态机实现报文高效解析

3. 路由层：RadixTree实现高性能路由匹配，支持多种路由模式，精准分发请求

4. 业务处理层：static_file_handler处理静态资源，middleware支持中间件扩展，实现通用业务逻辑

5. 入口调度层：HttpServer主类整合所有模块，对外提供启动、停止、路由注册等简洁接口

## 📝 示例用法（main.cc）

```
#include "src/http_server.h"
#include "src/static_file_handler.h"

int main() {
    // 初始化HTTP服务器
    HttpServer server;
    
    // 注册普通路由
    server.insert("GET", "/hello", [](const HttpRequest& req, HttpResponse* resp) {
        resp->set_content("Hello MyHttpServer!", "text/plain");
    });
    
    // 注册路径参数路由
    server.insert("GET", "/user/:id", [](const HttpRequest& req, HttpResponse* resp) {
        auto id = req.get_path_param("id");
        resp->set_content("User ID: " + std::string(id), "text/plain");
    });
    
    // 注册静态文件服务路由
    server.insert("GET", "/static/*", static_file_handler("./www"));
    
    // 启动服务器（默认监听80端口）
    server.start();
    return 0;
}
```

## 📌 优化亮点

- 状态机解析：HttpContext采用有限状态机，支持流式数据解析，适配分包到达场景，解析稳定

- 零拷贝传输：静态文件服务采用sendfile系统调用，减少用户态与内核态数据拷贝，提升传输效率

- 对象复用：HttpRequest、HttpResponse、HttpContext均提供reset方法，减少对象频繁创建销毁的性能开销

- 安全防护：utils模块提供路径安全检查，防止目录穿越攻击；请求解析过程中做长度限制，避免恶意请求崩溃

- 性能优化：编译开启-O2优化，状态码查询采用编译期初始化，路由匹配采用Radix树，大幅提升性能

## 📚 相关文档

项目详细优化原理、源码解析与实现细节，可参考我的技术博客：

[muduo http优化 —— 在原本数据监测http上 多支持了功能完善的http_1.1](https://blog.csdn.net/m0_62807361/article/details/159774170?spm=1001.2014.3001.5501)

## 📄 开源许可证

本项目采用 MIT License 开源，允许自由使用、修改、分发，无需承担额外责任。

## 💡 适用场景

- 高并发Web服务器、后端接口网关

- 静态资源服务器、文件下载服务

- 学习Reactor模型、HTTP/1.1协议、高并发编程的实践案例
