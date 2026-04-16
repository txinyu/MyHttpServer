// http_context.h
#ifndef HTTP_CONTEXT_H
#define HTTP_CONTEXT_H

#include "http_request.h"
#include <muduo/net/Buffer.h>
#include <memory>

class HttpContext {
public:
    enum ParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
        kError
    };

    HttpContext();
    ~HttpContext() = default;

    // 重置状态（用于复用连接上的多个请求）
    void reset();

    // 解析缓冲区中的数据，返回 false 表示解析过程中发生致命错误（如格式错误）
    // 成功解析完一个完整请求后，状态变为 kGotAll，此时可通过 request() 获取请求对象
    bool parseRequest(muduo::net::Buffer* buf);

    // 是否已获得完整请求
    bool gotAll() const { return state_ == kGotAll; }

    // 获取解析出的请求对象（仅在 gotAll() 为 true 时有效）
    const HttpRequest& request() const { return request_; }
    HttpRequest& request() { return request_; }   // 非 const
   // const HttpRequest& request() const { return request_; } // 可选提供 const 版本

    // 获取错误码（HTTP 状态码），仅在状态为 kError 时有效
    int errorCode() const { return error_code_; }

private:
    // 解析请求行
    bool parseRequestLine(muduo::net::Buffer* buf);

    // 解析头部
    bool parseHeaders(muduo::net::Buffer* buf);

    // 解析正文（根据 Content-Length 或 chunked）
    bool parseBody(muduo::net::Buffer* buf);

    // 解析 chunked 编码的正文
    bool parseChunkedBody(muduo::net::Buffer* buf);

    // 辅助函数：将查询字符串解析到 request_ 的 params_ 中
    void parseQueryString(const std::string& query);

    ParseState state_;
    HttpRequest request_;
    int error_code_;

    // 用于 body 解析的状态
    size_t expected_body_ = 0;       // Content-Length 期望的字节数
    bool chunked_ = false;            // 是否使用 chunked 编码
    size_t chunk_remain_ = 0;         // 当前 chunk 剩余未读字节数
};

#endif // HTTP_CONTEXT_H