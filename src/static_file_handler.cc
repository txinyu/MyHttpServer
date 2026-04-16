#include "static_file_handler.h"
#include "http_request.h"
#include "http_response.h"
#include "utils.h"
#include <muduo/net/Buffer.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <memory>

StaticFileHandler::StaticFileHandler(std::string root, std::string url_prefix,
                                     std::string cache_control)
    : root_(std::move(root)),
      url_prefix_(std::move(url_prefix)),
      cache_control_(std::move(cache_control)),
      directory_index_(false)
{
    // 确保根目录末尾没有斜杠
    if (!root_.empty() && root_.back() == '/')
        root_.pop_back();
}

bool StaticFileHandler::handle(const HttpRequest &req, HttpResponse *resp)
{
    // 检查 URL 前缀是否匹配
    const std::string &path = req.path();
    if (path.find(url_prefix_) != 0)
    {
        return false; // 不匹配，跳过
    }

    // 提取相对于 url_prefix 的请求路径
    std::string rel_path = path.substr(url_prefix_.size());
    if (rel_path.empty())
        rel_path = "/";

    // 安全检查：防止目录穿越
    if (!utils::is_safe_path(rel_path))
    {
        resp->set_status_code(403); // Forbidden
        resp->set_content("Forbidden", "text/plain");
        return true;
    }

    // 构建实际文件系统路径
    std::string full_path = getFilePath(rel_path);

    // 检查文件是否存在，是目录还是普通文件
    struct stat st;
    if (stat(full_path.c_str(), &st) != 0)
    {
        resp->set_status_code(404);
        resp->set_content("Not Found", "text/plain");
        return true;
    }

    // 处理目录
    if (S_ISDIR(st.st_mode))
    {
        // 尝试查找 index.html
        std::string index_path = full_path + "/index.html";
        if (stat(index_path.c_str(), &st) == 0 && S_ISREG(st.st_mode))
        {
            full_path = index_path;
            // 继续作为普通文件处理，此时 st 已更新为 index.html 的信息
        }
        else if (directory_index_)
        {
            // 生成目录索引
            std::string listing = generateDirectoryListing(full_path, rel_path);
            resp->set_status_code(200);
            resp->set_content(listing, "text/html");
            resp->set_header("Cache-Control", cache_control_);
            return true;
        }
        else
        {
            resp->set_status_code(403); // Forbidden (no index)
            resp->set_content("Directory listing not allowed", "text/plain");
            return true;
        }
    }

    // 确保是普通文件
    if (!S_ISREG(st.st_mode))
    {
        resp->set_status_code(403);
        resp->set_content("Forbidden", "text/plain");
        return true;
    }

    // 检查客户端缓存条件
    if (isNotModified(req, st))
    {
        resp->set_status_code(304);
        resp->set_header("Cache-Control", cache_control_);
        resp->set_header("ETag", generateETag(st));
        return true;
    }

    // 处理 Range 请求
    off_t offset = 0;
    size_t count = st.st_size;
    int range_result = parseRange(req, st.st_size, &offset, &count);
    if (range_result == -1)
    {
        // 无效 Range，返回 416
        resp->set_status_code(416);
        resp->set_content("Range Not Satisfiable", "text/plain");
        return true;
    }
    if (range_result == 1)
    {
        resp->set_status_code(206); // Partial Content
        resp->set_header("Content-Range", "bytes " + std::to_string(offset) +
                                              "-" + std::to_string(offset + count - 1) + "/" +
                                              std::to_string(st.st_size));
    }
    else
    {
        resp->set_status_code(200);
    }

    // 打开文件
    int fd = open(full_path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        resp->set_status_code(500);
        resp->set_content("Internal Server Error", "text/plain");
        return true;
    }

    // 使用 RAII 确保 fd 在异常时关闭
    // std::unique_ptr<int, decltype(&close)> fd_guard(&fd, [](int* p) { if (*p >= 0) close(*p); });
    // 管理文件描述符 RAII 正确写法
    std::unique_ptr<void, void (*)(void *)> fd_guard(
        reinterpret_cast<void *>(fd),
        [](void *p)
        {
            //int fd = reinterpret_cast<int>(p);
            int fd = static_cast<int>(reinterpret_cast<intptr_t>(p));
            if (fd >= 0)
                ::close(fd);
        });

    // 设置基本头部
    std::string mime = std::string(utils::mime_type(full_path));
    resp->set_header("Content-Type", mime);
    resp->set_header("Cache-Control", cache_control_);
    resp->set_header("ETag", generateETag(st));

    // 添加 Last-Modified（HTTP 日期格式）
    char time_buf[100];
    struct tm tm_buf;
    gmtime_r(&st.st_mtime, &tm_buf);
    strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm_buf);
    resp->set_header("Last-Modified", std::string_view(time_buf));

    // 使用零拷贝发送，转移 fd 所有权
    resp->set_file_send(fd, offset, count, mime);
    fd_guard.release(); // fd 所有权已转移，不再由 guard 关闭
    return true;
}

std::string StaticFileHandler::getFilePath(const std::string &request_path) const
{
    // 构建绝对路径，并规范化
    std::string result = root_;
    if (request_path.empty() || request_path == "/")
    {
        return result;
    }
    // 去除开头的 '/'
    size_t start = 0;
    if (request_path[0] == '/')
        start = 1;
    result += "/" + request_path.substr(start);
    return result;
}

std::string StaticFileHandler::generateETag(const struct stat &st) const
{
    // 使用 inode 和 mtime 生成弱 ETag
    // 格式: "W/\"inode-mtime-size\""
    return "W/\"" + std::to_string(st.st_ino) + "-" +
           std::to_string(st.st_mtime) + "-" +
           std::to_string(st.st_size) + "\"";
}

bool StaticFileHandler::isNotModified(const HttpRequest &req, const struct stat &st) const
{
    // 检查 If-None-Match
    std::string_view if_none_match = req.get_header("if-none-match");
    if (!if_none_match.empty())
    {
        std::string etag = generateETag(st);
        // 简单比较（忽略弱 ETag 前缀 W/）
        if (if_none_match == etag ||
            (if_none_match.size() > 2 && if_none_match.substr(2) == etag.substr(2)))
        {
            return true;
        }
    }

    // 检查 If-Modified-Since
    std::string_view if_modified_since = req.get_header("if-modified-since");
    if (!if_modified_since.empty())
    {
        struct tm tm = {};
        // 解析 HTTP 日期格式，简化处理，使用 strptime
        if (strptime(std::string(if_modified_since).c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm) != nullptr)
        {
            time_t mod_time = st.st_mtime;
            time_t if_time = timegm(&tm);
            if (mod_time <= if_time)
            {
                return true;
            }
        }
    }
    return false;
}

int StaticFileHandler::parseRange(const HttpRequest &req, off_t file_size,
                                  off_t *offset, size_t *count) const
{
    std::string_view range_header = req.get_header("range");
    if (range_header.empty())
    {
        return 0; // 无 Range
    }
    // 格式：bytes=start-end
    const std::string prefix = "bytes=";
    if (range_header.compare(0, prefix.size(), prefix) != 0)
    {
        return -1; // 格式错误
    }
    std::string_view range = range_header.substr(prefix.size());
    size_t dash = range.find('-');
    if (dash == std::string::npos)
    {
        return -1; // 格式错误
    }
    std::string_view start_str = range.substr(0, dash);
    std::string_view end_str = range.substr(dash + 1);
    off_t start, end;

    try
    {
        if (start_str.empty() && !end_str.empty())
        {
            // bytes=-500
            // off_t len = std::stoll(end_str);
            off_t len = std::stoll(std::string(end_str));
            if (len <= 0)
                return -1;
            start = file_size - len;
            end = file_size - 1;
            if (start < 0)
                start = 0;
        }
        else if (!start_str.empty() && end_str.empty())
        {
            // bytes=500-
            start = std::stoll(std::string(start_str));
            end = file_size - 1;
        }
        else if (!start_str.empty() && !end_str.empty())
        {
            // bytes=500-1000
            start = std::stoll(std::string(start_str));
            end = std::stoll(std::string(end_str));
        }
        else
        {
            // 同时为空，不合法
            return -1;
        }
    }
    catch (...)
    {
        return -1;
    }
    if (start < 0 || end >= file_size || start > end)
    {
        return -1; // 超出范围
    }
    *offset = start;
    *count = end - start + 1;
    return 1; // 有效 Range
}

std::string StaticFileHandler::generateDirectoryListing(const std::string &dir_path,
                                                        const std::string &request_path) const
{
    DIR *dir = opendir(dir_path.c_str());
    if (!dir)
    {
        return "<html><body><h1>403 Forbidden</h1></body></html>";
    }

    std::stringstream ss;
    ss << "<html><head><title>Index of " << escapeHtml(request_path) << "</title></head><body>";
    ss << "<h1>Index of " << escapeHtml(request_path) << "</h1><hr><ul>";

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr)
    {
        std::string name = ent->d_name;
        if (name == "." || name == "..")
            continue;
        // 拼接 URL
        std::string url = request_path;
        if (url.back() != '/')
            url += '/';
        url += name;
        // 添加链接，并转义文件名
        ss << "<li><a href=\"" << escapeHtml(url) << "\">" << escapeHtml(name) << "</a></li>";
    }
    closedir(dir);
    ss << "</ul><hr></body></html>";
    return ss.str();
}

std::string StaticFileHandler::escapeHtml(const std::string &s)
{
    std::string out;
    out.reserve(s.size() * 1.2);
    for (char c : s)
    {
        switch (c)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}