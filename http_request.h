// http_request.h
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <unordered_map>
#include <string_view>

class HttpRequest {
public:
    enum class Method {
        kInvalid,
        kGet,
        kPost,
        kPut,
        kDelete,
        kHead
    };

    HttpRequest() = default;
    
    // 重置所有成员，用于复用对象
    void reset();

    // ---------- 设置数据 ----------
    void set_method(Method method) { method_ = method; }
    void set_path(std::string_view path) { path_.assign(path); }
    void set_version(std::string_view version) { version_.assign(version); }
    void set_body(std::string&& body) { body_ = std::move(body); }
    void set_body(const std::string& body) { body_ = body; }

    // 头部操作
    void set_header(std::string_view key, std::string_view value);
    bool has_header(std::string_view key) const;
    std::string_view get_header(std::string_view key) const;
    const std::unordered_map<std::string, std::string>& headers() const { return headers_; }

    // 查询参数（URL ? 后面的部分）
    void set_param(std::string_view key, std::string_view value);
    bool has_param(std::string_view key) const;
    std::string_view get_param(std::string_view key) const;

    // 路径参数（如 /user/:id 中的 id）
    void set_path_param(std::string_view key, std::string_view value);
    bool has_path_param(std::string_view key) const;
    std::string_view get_path_param(std::string_view key) const;

    // ---------- 获取数据 ----------
    Method method() const { return method_; }
    const std::string& path() const { return path_; }
    const std::string& version() const { return version_; }
    const std::string& body() const { return body_; }

    // 辅助方法
    bool is_keep_alive() const;
    size_t content_length() const;

private:
    Method method_ = Method::kInvalid;
    std::string path_;
    std::string version_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> params_;      // 查询参数
    std::unordered_map<std::string, std::string> path_params_; // 路由参数
};

#endif // HTTP_REQUEST_H