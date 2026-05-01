#include "http_server.h"
#include "http_request.h"
#include "http_response.h"
#include "http_context.h"
#include "radix_tree.h"
#include "middleware.h"
#include "static_file_handler.h"
#include "thread_pool.h"
#include "utils.h"
#include <muduo/net/Buffer.h>
#include <muduo/base/Logging.h>
#include <boost/any.hpp>
#include <functional>
#include <memory>
#include <sys/sendfile.h>
#include <unistd.h>
#include <cerrno>
#include <thread>

using namespace muduo::net;
using namespace std::placeholders;

HttpServer::HttpServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const std::string& name)
    : loop_(loop),
      server_(loop, listenAddr, name),
      router_(std::make_unique<RadixTree>()),
      middlewareChain_(std::make_unique<MiddlewareChain>()) {
    server_.setConnectionCallback(std::bind(&HttpServer::onConnection, this, _1));
    server_.setMessageCallback(std::bind(&HttpServer::onMessage, this, _1, _2, _3));
}

HttpServer::~HttpServer() = default;

void HttpServer::start() {
    server_.start();
}

void HttpServer::setIoThreads(int numThreads) {
    server_.setThreadNum(numThreads);
}

void HttpServer::get(const std::string& path, HttpCallback cb) {
    router_->insert("GET", path, std::move(cb));
}

void HttpServer::post(const std::string& path, HttpCallback cb) {
    router_->insert("POST", path, std::move(cb));
}

void HttpServer::put(const std::string& path, HttpCallback cb) {
    router_->insert("PUT", path, std::move(cb));
}

void HttpServer::del(const std::string& path, HttpCallback cb) {
    router_->insert("DELETE", path, std::move(cb));
}

void HttpServer::serveStatic(const std::string& urlPrefix,
                             const std::string& fsRoot,
                             const std::string& cacheControl) {
    auto handler = std::make_unique<StaticFileHandler>(fsRoot, urlPrefix, cacheControl);
    staticHandlers_.push_back(std::move(handler));
}

void HttpServer::use(Middleware mw) {
    middlewareChain_->add(std::move(mw));
}

void HttpServer::enableAsync(size_t threadCount) {
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 2;
    }
    threadPool_ = std::make_unique<ThreadPool>(threadCount);
}

void HttpServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        conn->setContext(HttpContext());
    }
}

void HttpServer::onMessage(const muduo::net::TcpConnectionPtr& conn,
                           muduo::net::Buffer* buf,
                           muduo::Timestamp receiveTime) {
    HttpContext* context = boost::any_cast<HttpContext>(conn->getMutableContext());
    bool shouldClose = false;

    while (buf->readableBytes() > 0 && !shouldClose) {
        if (!context->parseRequest(buf)) {
            if (context->errorCode() != 200) {
                HttpResponse resp(true);
                resp.set_status_code(context->errorCode());
                resp.set_content(utils::status_text(context->errorCode()), "text/plain");
                sendResponse(conn, context->request(), resp);
                shouldClose = true;
            }
            context->reset();
            break;
        }
        if (context->gotAll()) {
            bool reqClose = handleRequest(conn, context);
            context->reset();
            if (reqClose) shouldClose = true;
        } else {
            break;
        }
    }

    if (shouldClose) {
        conn->shutdown();
    }
}

bool HttpServer::handleRequest(const TcpConnectionPtr& conn, HttpContext* context) {
    HttpRequest& req = context->request();
    HttpResponse resp(!req.is_keep_alive());

    // 请求体大小限制（假设最大 10MB）
    const size_t maxBodySize = 10 * 1024 * 1024;
    if (req.content_length() > maxBodySize) {
        resp.set_status_code(413);
        resp.set_content("Payload Too Large", "text/plain");
        sendResponse(conn, req, resp);
        return resp.should_close();
    }

    // 静态文件优先
    bool handled = false;
    for (auto& handler : staticHandlers_) {
        if (handler->handle(req, &resp)) {
            handled = true;
            break;
        }
    }

    if (!handled) {
        if (threadPool_) {
            // 异步模式：拷贝请求和响应，在独立线程处理
            auto shared_req = std::make_shared<HttpRequest>(std::move(req));
            auto shared_resp = std::make_shared<HttpResponse>(std::move(resp));
            threadPool_->submit([this, conn, shared_req, shared_resp]() {
                try {
                    route(*shared_req, shared_resp.get());
                } catch (const std::exception& e) {
                    LOG_ERROR << "Exception in async route: " << e.what();
                    shared_resp->set_status_code(500);
                    shared_resp->set_content("Internal Server Error", "text/plain");
                }
                loop_->runInLoop([this, conn, shared_req, shared_resp]() {
                    if (conn->connected()) {
                        HttpResponse respCopy = *shared_resp;
                        sendResponse(conn, *shared_req, respCopy);
                        if (respCopy.should_close()) {
                            conn->shutdown();
                        }
                    }
                });
            });
            // 异步模式，连接关闭由异步回调决定，这里不立即关闭
            return false;
        } else {
            // 同步模式
            route(req, &resp);
        }
    }

    sendResponse(conn, req, resp);
    return resp.should_close();
}

void HttpServer::route(HttpRequest& req, HttpResponse* resp) {
    middlewareChain_->run(req, *resp, [&]() {
        HttpHandler handler;
        std::unordered_map<std::string, std::string> params;
        std::string method;
        switch (req.method()) {
            case HttpRequest::Method::kGet:
                method = "GET";
                break;
            case HttpRequest::Method::kPost:
                method = "POST";
                break;
            case HttpRequest::Method::kPut:
                method = "PUT";
                break;
            case HttpRequest::Method::kDelete:
                method = "DELETE";
                break;
            default:
                method = "";
                break;
        }
        if (!method.empty() && router_->route(method, req.path(), &handler, &params)) {
            for (const auto& p : params) {
                req.set_path_param(p.first, p.second);
            }
            handler(req, resp);
        } else {
            resp->set_status_code(404);
            resp->set_content("Not Found", "text/plain");
        }
    });
}

void HttpServer::sendResponse(const TcpConnectionPtr& conn,
                              const HttpRequest& req,
                              HttpResponse& resp) {
    // 补充 Connection 头
    if (req.is_keep_alive() && !resp.should_close()) {
        resp.set_header("Connection", std::string("keep-alive"));
    } else {
        resp.set_header("Connection", std::string("close"));
    }

    // 序列化头部
    muduo::net::Buffer buf;
    resp.serialize_to(&buf);
    conn->send(&buf);

    // 处理文件响应（使用 read + send，避免依赖 fd()）
    if (resp.is_file_response()) {
        int fd = resp.file_fd();
        off_t offset = resp.file_offset();
        size_t count = resp.file_count();
        if (fd >= 0 && count > 0) {
            const size_t chunkSize = 8192;
            char buffer[chunkSize];
            off_t remain = count;
            off_t cur = offset;
            while (remain > 0) {
                size_t toRead = std::min(static_cast<size_t>(remain), chunkSize);
                ssize_t n = pread(fd, buffer, toRead, cur);
                if (n < 0) {
                    LOG_ERROR << "read file error: " << strerror(errno);
                    break;
                } else if (n == 0) {
                    break;
                }
                conn->send(buffer, n);
                remain -= n;
                cur += n;
            }
            ::close(fd);
        }
    }
}
