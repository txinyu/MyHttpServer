#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>

class HttpRequest;
class HttpResponse;
class HttpContext;
class RadixTree;
class MiddlewareChain;
class StaticFileHandler;
class ThreadPool;

using Next = std::function<void()>;
using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;
using Middleware = std::function<bool(HttpRequest&, HttpResponse&, Next)>;

class HttpServer {
public:
    HttpServer(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& listenAddr,
               const std::string& name);
    ~HttpServer();

    // 禁止拷贝
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void start();
    void setIoThreads(int numThreads);

    void get(const std::string& path, HttpCallback cb);
    void post(const std::string& path, HttpCallback cb);
    void put(const std::string& path, HttpCallback cb);
    void del(const std::string& path, HttpCallback cb);

    void serveStatic(const std::string& urlPrefix, const std::string& fsRoot,
                     const std::string& cacheControl = "max-age=3600");

    void use(Middleware mw);
    void enableAsync(size_t threadCount = 0);

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp receiveTime);
    bool handleRequest(const muduo::net::TcpConnectionPtr& conn, HttpContext* context);
    void sendResponse(const muduo::net::TcpConnectionPtr& conn,
                      const HttpRequest& req,
                      HttpResponse& resp);
    void route(HttpRequest& req, HttpResponse* resp);

    // 发送文件内容（零拷贝）
    bool sendFileContent(const muduo::net::TcpConnectionPtr& conn,
                         int fd, off_t offset, size_t count);

    muduo::net::EventLoop* loop_;
    muduo::net::TcpServer server_;

    std::unique_ptr<RadixTree> router_;
    std::unique_ptr<MiddlewareChain> middlewareChain_;
    std::vector<std::unique_ptr<StaticFileHandler>> staticHandlers_;
    std::unique_ptr<ThreadPool> threadPool_;
};

#endif // HTTP_SERVER_H