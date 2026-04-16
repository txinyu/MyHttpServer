// utils.h
#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace utils {

// ========== HTTP 状态码 ==========
// 获取状态码对应的标准描述文本（如 200 -> "OK"）
std::string_view status_text(int code);

// ========== MIME 类型 ==========
// 根据文件名（或扩展名）获取 MIME 类型，若未知则返回 "application/octet-stream"
std::string_view mime_type(std::string_view filename);

// ========== URL 编解码 ==========
// 对字符串进行 URL 编码（RFC 3986）
// space_to_plus: 是否将空格编码为 '+'（用于查询字符串）
std::string url_encode(std::string_view str, bool space_to_plus = true);

// 对字符串进行 URL 解码
// plus_to_space: 是否将 '+' 解码为空格（用于查询字符串）
std::string url_decode(std::string_view str, bool plus_to_space = true);

// ========== 文件系统工具 ==========
// 判断路径是否为普通文件
bool is_regular_file(const std::string& path);

// 判断路径是否为目录
bool is_directory(const std::string& path);

// 检查路径是否安全（防止目录穿越，如包含 ".." 试图跳出根目录）
bool is_safe_path(const std::string& path);

// 读取整个文件内容到字符串（仅用于小型文件，静态文件应优先使用 sendfile）
bool read_file(const std::string& path, std::string* out);

// 写入字符串到文件（覆盖）
bool write_file(const std::string& path, const std::string& content);

// ========== 字符串工具 ==========
// 按分隔符分割字符串，结果存入 vector
size_t split(std::string_view str, std::string_view sep, std::vector<std::string>* out);

} // namespace utils

#endif // UTILS_H