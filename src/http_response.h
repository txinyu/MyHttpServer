// http_response.h
#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <unordered_map>
#include <string_view>
#include <memory>

namespace muduo {
namespace net {
class Buffer;
}
}

class HttpResponse {
public:
    // 常用状态码常量
    enum HttpStatusCode {
        k200Ok = 200,
        k206PartialContent = 206,
        k301MovedPermanently = 301,
        k302Found = 302,
        k304NotModified = 304,
        k400BadRequest = 400,
        k403Forbidden = 403,
        k404NotFound = 404,
        k405MethodNotAllowed = 405,
        k500InternalServerError = 500,
    };

    HttpResponse();
    explicit HttpResponse(bool close_connection);
    
    // 重置对象（用于复用）
    void reset();

    // 设置状态码（自动填充状态消息）
    void set_status_code(int code);
    // 设置状态码和自定义消息
    void set_status_code(int code, std::string_view message);

    // 设置头部字段
    void set_header(const std::string& key, const std::string& value);
    void set_header(std::string_view key, std::string_view value);

    // 获取头部字段值（用于内部判断）
    std::string get_header(const std::string& key) const;

    // 设置响应正文（同时自动添加 Content-Type 和 Content-Length）
    void set_content(std::string_view body, std::string_view content_type = "text/html");

    // 设置重定向
    void set_redirect(std::string_view url, int status_code = k302Found);

    // 设置通过文件描述符发送（零拷贝）
    // 调用者负责管理 fd 的生命周期，通常应在发送后关闭
    void set_file_send(int fd, off_t offset, size_t count, const std::string& content_type);

    // 是否关闭连接（根据 Connection 头和版本判断，由服务器调用）
    void set_close_connection(bool close) { close_connection_ = close; }
    bool should_close() const { return close_connection_; }

    // 将响应序列化到 muduo::net::Buffer 中（用于普通正文响应）
    void serialize_to(muduo::net::Buffer* output) const;

    // 零拷贝发送相关访问器（供 TcpConnection 使用）
    int file_fd() const { return file_fd_; }
    off_t file_offset() const { return file_offset_; }
    size_t file_count() const { return file_count_; }
    bool is_file_response() const { return file_fd_ >= 0; }

    // 获取状态码
    int status_code() const { return status_code_; }
    const std::string& status_message() const { return status_message_; }

private:
    // 构建响应行和头部，写入 Buffer（不包含正文）
    void serialize_headers(muduo::net::Buffer* output) const;

    int status_code_;
    std::string status_message_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;              // 正文（用于非零拷贝响应）
    bool close_connection_;         // 是否在发送后关闭连接

    // 零拷贝文件发送相关
    int file_fd_ = -1;
    off_t file_offset_ = 0;
    size_t file_count_ = 0;
};

#endif // HTTP_RESPONSE_H