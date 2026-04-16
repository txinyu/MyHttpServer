// http_response.cc
#include "http_response.h"
#include "utils.h"
#include <muduo/net/Buffer.h>
#include <sstream>
#include <algorithm>

HttpResponse::HttpResponse()
    : status_code_(k200Ok),
      status_message_(utils::status_text(k200Ok)),
      close_connection_(false) {
}

HttpResponse::HttpResponse(bool close_connection)
    : status_code_(k200Ok),
      status_message_(utils::status_text(k200Ok)),
      close_connection_(close_connection) {
}

void HttpResponse::reset() {
    status_code_ = k200Ok;
    status_message_ = utils::status_text(k200Ok);
    headers_.clear();
    body_.clear();
    close_connection_ = false;
    if (file_fd_ >= 0) {
        // 注意：这里不关闭 fd，应由调用者负责
        file_fd_ = -1;
    }
    file_offset_ = 0;
    file_count_ = 0;
}

void HttpResponse::set_status_code(int code) {
    status_code_ = code;
    status_message_ = utils::status_text(code);
}

void HttpResponse::set_status_code(int code, std::string_view message) {
    status_code_ = code;
    status_message_ = message;
}

void HttpResponse::set_header(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpResponse::set_header(std::string_view key, std::string_view value) {
    headers_[std::string(key)] = std::string(value);
}

std::string HttpResponse::get_header(const std::string& key) const {
    auto it = headers_.find(key);
    if (it == headers_.end())
        return "";
    return it->second;
}

void HttpResponse::set_content(std::string_view body, std::string_view content_type) {
    body_.assign(body.data(), body.size());
    set_header("Content-Type", content_type);
    // Content-Length 会在序列化时根据 body 大小自动添加
}

void HttpResponse::set_redirect(std::string_view url, int status_code) {
    set_status_code(status_code);
    set_header("Location", url);
    body_.clear();  // 重定向通常无正文
}

void HttpResponse::set_file_send(int fd, off_t offset, size_t count, const std::string& content_type) {
    file_fd_ = fd;
    file_offset_ = offset;
    file_count_ = count;
    set_header("Content-Type", content_type);
    // Content-Length 会在序列化时根据 file_count 添加
}

/*void HttpResponse::serialize_to(muduo::net::Buffer* output) const {
    // 如果不是文件响应，且 body 为空，且没有 Content-Length 头，则自动设置为 0
//    if (!is_file_response() && body_.empty() && headers_.find("Content-Length") == headers_.end()) {
 //       // 对于 304 等无正文响应，不应添加 Content-Length
 //       if (status_code_ != k304NotModified) {
 //           const_cast<HttpResponse*>(this)->set_header("Content-Length", "0");
  //      }
 //   }

    // 1. 写入响应行
    std::string line = "HTTP/1.1 " + std::to_string(status_code_) + " " + status_message_ + "\r\n";
    output->append(line);

    // 2. 写入头部（注意：Content-Length 可能尚未设置，但会在下面处理）
    for (const auto& kv : headers_) {
        output->append(kv.first + ": " + kv.second + "\r\n");
    }

    if (!is_file_response() && body_.empty() && headers_.find("Content-Length") == headers_.end()) {
        if (status_code_ != k304NotModified) {
            output->append("Content-Length: 0\r\n");
        }
    }

    // 3. 写入额外的 Content-Length（若尚未存在且需要）
    if (!is_file_response()) {
        if (headers_.find("Content-Length") == headers_.end() && !body_.empty()) {
            output->append("Content-Length: " + std::to_string(body_.size()) + "\r\n");
        }
    } else {
        // 文件响应：确保有 Content-Length
        if (headers_.find("Content-Length") == headers_.end()) {
            output->append("Content-Length: " + std::to_string(file_count_) + "\r\n");
        }
    }

    // 4. 空行结束头部
    output->append("\r\n");

    // 5. 写入正文（仅当不是文件响应且不是无正文状态码）
    if (!is_file_response() && !body_.empty() && status_code_ != k304NotModified) {
        output->append(body_);
    }
}

*/
void HttpResponse::serialize_to(muduo::net::Buffer* output) const {
    // 1. 响应行
    std::string line = "HTTP/1.1 " + std::to_string(status_code_) + " " + status_message_ + "\r\n";
    output->append(line);

    // 2. 写入现有头部
    for (const auto& kv : headers_) {
        output->append(kv.first + ": " + kv.second + "\r\n");
    }

    // 3. 自动补充 Content-Length（如果缺失且必要）
    bool need_content_length = false;
    std::string content_length_value;
    if (!is_file_response() && headers_.find("Content-Length") == headers_.end()) {
        if (!body_.empty()) {
            content_length_value = std::to_string(body_.size());
            need_content_length = true;
        } else if (status_code_ != k304NotModified) {
            // 空 body，添加 Content-Length: 0
            content_length_value = "0";
            need_content_length = true;
        }
    } else if (is_file_response() && headers_.find("Content-Length") == headers_.end()) {
        content_length_value = std::to_string(file_count_);
        need_content_length = true;
    }
    if (need_content_length) {
        output->append("Content-Length: " + content_length_value + "\r\n");
    }

    // 4. 空行结束头部
    output->append("\r\n");

    // 5. 正文（非文件响应且有内容）
    if (!is_file_response() && !body_.empty() && status_code_ != k304NotModified) {
        output->append(body_);
    }
}
void HttpResponse::serialize_headers(muduo::net::Buffer* output) const {
    // 仅序列化头部（不含响应行和空行），供内部使用
    for (const auto& kv : headers_) {
        output->append(kv.first + ": " + kv.second + "\r\n");
    }
}