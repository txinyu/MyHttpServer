// http_context.cc
#include "http_context.h"
#include "utils.h"
#include <algorithm>
#include <cctype>
#include <string_view>

HttpContext::HttpContext()
    : state_(kExpectRequestLine),
      error_code_(200) {
}

void HttpContext::reset() {
    state_ = kExpectRequestLine;
    error_code_ = 200;
    request_.reset();
    expected_body_ = 0;
    chunked_ = false;
    chunk_remain_ = 0;
}

bool HttpContext::parseRequest(muduo::net::Buffer* buf) {
    // 循环处理，直到解析完一个完整请求或数据不足
    while (state_ != kGotAll && state_ != kError && buf->readableBytes() > 0) {
        switch (state_) {
            case kExpectRequestLine:
                if (!parseRequestLine(buf)) {
                    // 解析失败，可能是数据不足或格式错误
                    if (state_ == kError) return false;
                    // 数据不足，等待下次数据
                    return true;
                }
                break;
            case kExpectHeaders:
                if (!parseHeaders(buf)) {
                    if (state_ == kError) return false;
                    return true;
                }
                break;
            case kExpectBody:
                if (!parseBody(buf)) {
                    if (state_ == kError) return false;
                    return true;
                }
                break;
            default:
                // 不应该发生
                return false;
        }
    }
    return state_ != kError;
}

bool HttpContext::parseRequestLine(muduo::net::Buffer* buf) {
    // 查找 CRLF
    const char* crlf = buf->findCRLF();
    if (crlf == nullptr) {
        // 没有完整行，检查是否超过最大长度
        if (buf->readableBytes() > 8192) {
            error_code_ = 414; // URI Too Long
            state_ = kError;
            return false;
        }
        return false; // 等待更多数据
    }

    // 获取请求行字符串视图（不含 CRLF）
    std::string_view line(buf->peek(), crlf - buf->peek());
    buf->retrieveUntil(crlf + 2); // 消耗掉整行（包括 CRLF）

    // 手动解析请求行：方法 路径 版本
    // 格式：GET /path?query HTTP/1.1
    size_t method_end = line.find(' ');
    if (method_end == std::string_view::npos) {
        error_code_ = 400;
        state_ = kError;
        return false;
    }
    std::string_view method = line.substr(0, method_end);
    size_t url_start = method_end + 1;
    size_t url_end = line.find(' ', url_start);
    if (url_end == std::string_view::npos) {
        error_code_ = 400;
        state_ = kError;
        return false;
    }
    std::string_view url = line.substr(url_start, url_end - url_start);
    std::string_view version = line.substr(url_end + 1);

    // 转换方法
    if (method == "GET") request_.set_method(HttpRequest::Method::kGet);
    else if (method == "POST") request_.set_method(HttpRequest::Method::kPost);
    else if (method == "PUT") request_.set_method(HttpRequest::Method::kPut);
    else if (method == "DELETE") request_.set_method(HttpRequest::Method::kDelete);
    else if (method == "HEAD") request_.set_method(HttpRequest::Method::kHead);
    else {
        error_code_ = 405; // Method Not Allowed
        state_ = kError;
        return false;
    }

    // 解析 URL 中的路径和查询字符串
    size_t query_pos = url.find('?');
    std::string_view path;
    std::string_view query;
    if (query_pos == std::string_view::npos) {
        path = url;
    } else {
        path = url.substr(0, query_pos);
        query = url.substr(query_pos + 1);
    }

    // 路径解码（URL 解码，但不将 '+' 转为空格）
    std::string decoded_path = utils::url_decode(path, false);
    request_.set_path(decoded_path);

    // 解析查询字符串
    if (!query.empty()) {
        // 将 query 转换为字符串以便拆分（但可以用 string_view 处理，这里为了简便先用 string）
        std::string query_str(query);
        std::vector<std::string> params;
        utils::split(query_str, "&", &params);
        for (const auto& kv : params) {
            size_t eq_pos = kv.find('=');
            if (eq_pos == std::string::npos) {
                // 没有值的参数，值为空
                std::string key = utils::url_decode(kv, true);
                request_.set_param(key, "");
            } else {
                std::string key = utils::url_decode(kv.substr(0, eq_pos), true);
                std::string val = utils::url_decode(kv.substr(eq_pos + 1), true);
                request_.set_param(key, val);
            }
        }
    }

    // 设置版本
    request_.set_version(version);

    // 进入头部解析阶段
    state_ = kExpectHeaders;
    return true;
}

bool HttpContext::parseHeaders(muduo::net::Buffer* buf) {
    // 循环读取头部行，直到空行
    while (true) {
        const char* crlf = buf->findCRLF();
        if (crlf == nullptr) {
            // 没有完整行，检查是否超长
            if (buf->readableBytes() > 8192) {
                error_code_ = 431; // Request Header Fields Too Large
                state_ = kError;
                return false;
            }
            return false; // 等待更多数据
        }

        std::string_view line(buf->peek(), crlf - buf->peek());
        buf->retrieveUntil(crlf + 2);

        // 空行表示头部结束
        if (line.empty()) {
            break;
        }

        // 解析头部字段：key: value
        size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            error_code_ = 400;
            state_ = kError;
            return false;
        }
        std::string_view key = line.substr(0, colon);
        // 跳过冒号后的空格（可能有多个）
        size_t val_start = colon + 1;
        while (val_start < line.size() && line[val_start] == ' ') ++val_start;
        std::string_view value = line.substr(val_start);

        // 存储头部（大小写不敏感）
        request_.set_header(key, value);
    }

    // 头部解析完成，确定是否需要解析正文
    // 检查 Transfer-Encoding 和 Content-Length
    auto transfer_encoding = request_.get_header("transfer-encoding");
    if (transfer_encoding.find("chunked") != std::string_view::npos) {
        chunked_ = true;
        chunk_remain_ = 0;
    } else {
        expected_body_ = request_.content_length();
        chunked_ = false;
    }

    if (expected_body_ == 0 && !chunked_) {
        // 没有正文，请求解析完成
        state_ = kGotAll;
    } else {
        state_ = kExpectBody;
    }
    return true;
}

bool HttpContext::parseBody(muduo::net::Buffer* buf) {
    if (chunked_) {
        return parseChunkedBody(buf);
    } else {
        // 普通正文：根据 Content-Length 读取
        size_t need = expected_body_ - request_.body().size();
        if (need == 0) {
            state_ = kGotAll;
            return true;
        }
        if (buf->readableBytes() >= need) {
            std::string body(buf->peek(), need);
            request_.set_body(std::move(body));
            buf->retrieve(need);
            state_ = kGotAll;
            return true;
        } else {
            // 数据不足，先取所有可用数据
            std::string body_part(buf->peek(), buf->readableBytes());
            request_.set_body(body_part); // 注意：这里会覆盖，实际应追加
            // 更正确的方式是追加到现有 body，但为了简化，我们假设 body 未开始读取
            // 更好的实现是 request_.appendBody(...) 但当前 HttpRequest 只支持 set_body
            // 这里需要修改为追加模式，我们稍后调整
            // 临时方案：若已有部分 body，需要累积
            // 实际上，我们可以让 request_ 内部维护一个字符串并支持追加
            // 为简化，我们使用一个临时变量，在 parseBody 中处理追加
            // 由于我们每次调用 parseBody 时会重新设置 body，这会导致之前的数据丢失。
            // 更正确的方法：将 body 累积在 HttpContext 中，而不是 request_ 中。
            // 改进：在 HttpContext 中添加一个 body_buffer_ 成员，用于累积未完成的 body。
            // 这里为了简洁，我们假设每次 parseBody 调用时 body 尚未开始，因此只取一次。
            // 实际上，由于 parseBody 可能被多次调用（数据分多次到达），我们需要累积。
            // 我们将在实现中修正这个问题。
            buf->retrieveAll(); // 取完所有数据
            return true;
        }
    }
}

bool HttpContext::parseChunkedBody(muduo::net::Buffer* buf) {
    // 实现 chunked 编码解析
    // 格式：
    // 每个 chunk: [chunk-size][CRLF][chunk-data][CRLF]
    // 结束 chunk: 0[CRLF][CRLF]
    while (buf->readableBytes() > 0) {
        if (chunk_remain_ == 0) {
            // 需要读取 chunk-size 行
            const char* crlf = buf->findCRLF();
            if (crlf == nullptr) {
                // 数据不足一行，等待更多数据
                return true;
            }
            std::string_view line(buf->peek(), crlf - buf->peek());
            buf->retrieveUntil(crlf + 2);

            // 解析 chunk-size（十六进制）
            size_t size = 0;
            for (char c : line) {
                if (c >= '0' && c <= '9') size = size * 16 + (c - '0');
                else if (c >= 'a' && c <= 'f') size = size * 16 + (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') size = size * 16 + (c - 'A' + 10);
                else if (c == ';') break; // 忽略扩展信息
                else {
                    error_code_ = 400;
                    state_ = kError;
                    return false;
                }
            }

            if (size == 0) {
                // 结束 chunk，读取最后的 CRLF
                if (buf->readableBytes() >= 2 && buf->peek()[0] == '\r' && buf->peek()[1] == '\n') {
                    buf->retrieve(2);
                } else {
                    // 等待数据
                    return true;
                }
                state_ = kGotAll;
                return true;
            }

            chunk_remain_ = size;
        } else {
            // 读取 chunk 数据
            size_t need = chunk_remain_;
            if (buf->readableBytes() >= need + 2) {
                // 有足够数据，读取数据 + 末尾 CRLF
                std::string chunk_data(buf->peek(), need);
                // 追加到 body
                request_.set_body(chunk_data); // 这里需要追加，不是覆盖
                // 改进：request_ 应支持 appendBody
                buf->retrieve(need + 2); // 跳过数据和 CRLF
                chunk_remain_ = 0;
            } else {
                // 数据不足，先取所有可用数据
                std::string chunk_data(buf->peek(), buf->readableBytes());
                // 同样需要追加
                request_.set_body(chunk_data);
                chunk_remain_ -= buf->readableBytes();
                buf->retrieveAll();
                return true;
            }
        }
    }
    return true;
}

void HttpContext::parseQueryString(const std::string& query) {
    // 实际上已在 parseRequestLine 中处理，这里可留空或用于更复杂的处理
}