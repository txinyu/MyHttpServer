// http_request.cc
#include "http_request.h"
#include <cctype>
#include <algorithm>

void HttpRequest::reset() {
    method_ = Method::kInvalid;
    path_.clear();
    version_.clear();
    body_.clear();
    headers_.clear();
    params_.clear();
    path_params_.clear();
}

void HttpRequest::set_header(std::string_view key, std::string_view value) {
    // 存储时统一转为小写，便于大小写不敏感查找
    std::string key_lower(key);
    std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);
    headers_[std::move(key_lower)] = std::string(value);
}

bool HttpRequest::has_header(std::string_view key) const {
    std::string key_lower(key);
    std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);
    return headers_.find(key_lower) != headers_.end();
}

std::string_view HttpRequest::get_header(std::string_view key) const {
    std::string key_lower(key);
    std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);
    auto it = headers_.find(key_lower);
    if (it == headers_.end())
        return {};
    return it->second;
}

void HttpRequest::set_param(std::string_view key, std::string_view value) {
    params_[std::string(key)] = std::string(value);
}

bool HttpRequest::has_param(std::string_view key) const {
    return params_.find(std::string(key)) != params_.end();
}

std::string_view HttpRequest::get_param(std::string_view key) const {
    auto it = params_.find(std::string(key));
    if (it == params_.end())
        return {};
    return it->second;
}

void HttpRequest::set_path_param(std::string_view key, std::string_view value) {
    path_params_[std::string(key)] = std::string(value);
}

bool HttpRequest::has_path_param(std::string_view key) const {
    return path_params_.find(std::string(key)) != path_params_.end();
}

std::string_view HttpRequest::get_path_param(std::string_view key) const {
    auto it = path_params_.find(std::string(key));
    if (it == path_params_.end())
        return {};
    return it->second;
}

bool HttpRequest::is_keep_alive() const {
    auto connection = get_header("connection");
    if (connection.empty()) {
        // HTTP/1.0 默认 close，HTTP/1.1 默认 keep-alive
        if (version_ == "HTTP/1.1")
            return true;
        else
            return false;
    }
    // 大小写不敏感比较
    std::string conn(connection);
    std::transform(conn.begin(), conn.end(), conn.begin(), ::tolower);
    return conn == "keep-alive";
}

size_t HttpRequest::content_length() const {
    auto clen = get_header("content-length");
    if (clen.empty())
        return 0;
    try {
        return std::stoul(std::string(clen));
    } catch (...) {
        return 0;
    }
}