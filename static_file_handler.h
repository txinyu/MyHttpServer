#ifndef STATIC_FILE_HANDLER_H
#define STATIC_FILE_HANDLER_H

#include <string>
#include <sys/types.h>

class HttpRequest;
class HttpResponse;

class StaticFileHandler {
public:
    // 构造函数
    // root: 文件系统根目录（如 "/var/www/html"）
    // url_prefix: URL 前缀（如 "/static"），只有以此开头的请求才会被处理
    // cache_control: 缓存控制头，如 "max-age=3600" 或 "no-cache"
    StaticFileHandler(std::string root, std::string url_prefix,
                      std::string cache_control = "max-age=3600");

    // 处理请求，填充响应
    // 如果请求应由此 handler 处理，返回 true；否则返回 false（URL 前缀不匹配）
    bool handle(const HttpRequest& req, HttpResponse* resp);

    // 设置是否生成目录索引（当请求目录且无 index.html 时）
    void set_directory_index(bool enable) { directory_index_ = enable; }

private:
    // 获取文件的绝对路径（结合根目录和请求路径，并防止目录穿越）
    std::string getFilePath(const std::string& request_path) const;

    // 生成文件的 ETag（基于 inode 和 mtime）
    std::string generateETag(const struct stat& st) const;

    // 检查客户端缓存是否有效（If-None-Match / If-Modified-Since）
    bool isNotModified(const HttpRequest& req, const struct stat& st) const;

    // 解析 Range 请求，返回结果类型和范围
    // 返回值: 0 - 无 Range 头; 1 - 有效 Range; -1 - 无效 Range
    int parseRange(const HttpRequest& req, off_t file_size,
                   off_t* offset, size_t* count) const;

    // 生成目录索引页面（HTML 格式）
    std::string generateDirectoryListing(const std::string& dir_path,
                                         const std::string& request_path) const;

    // HTML 转义
    static std::string escapeHtml(const std::string& s);

    std::string root_;           // 文件系统根目录
    std::string url_prefix_;     // URL 前缀
    std::string cache_control_;  // Cache-Control 头
    bool directory_index_;       // 是否生成目录索引
};

#endif // STATIC_FILE_HANDLER_H