// utils.cc
#include "utils.h"
#include <array>
#include <unordered_map>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <system_error>
#include <cstdio>

namespace fs = std::filesystem;

namespace utils {

// ========== 状态码表 ==========
// 使用 constexpr array 实现 O(1) 索引访问，编译期确定
static constexpr std::array<std::string_view, 512> status_texts = [] {
    std::array<std::string_view, 512> arr{};
    // 1xx
    arr[100] = "Continue";
    arr[101] = "Switching Protocols";
    // 2xx
    arr[200] = "OK";
    arr[201] = "Created";
    arr[202] = "Accepted";
    arr[203] = "Non-Authoritative Information";
    arr[204] = "No Content";
    arr[205] = "Reset Content";
    arr[206] = "Partial Content";
    // 3xx
    arr[300] = "Multiple Choices";
    arr[301] = "Moved Permanently";
    arr[302] = "Found";
    arr[303] = "See Other";
    arr[304] = "Not Modified";
    arr[305] = "Use Proxy";
    arr[307] = "Temporary Redirect";
    arr[308] = "Permanent Redirect";
    // 4xx
    arr[400] = "Bad Request";
    arr[401] = "Unauthorized";
    arr[402] = "Payment Required";
    arr[403] = "Forbidden";
    arr[404] = "Not Found";
    arr[405] = "Method Not Allowed";
    arr[406] = "Not Acceptable";
    arr[407] = "Proxy Authentication Required";
    arr[408] = "Request Timeout";
    arr[409] = "Conflict";
    arr[410] = "Gone";
    arr[411] = "Length Required";
    arr[412] = "Precondition Failed";
    arr[413] = "Payload Too Large";
    arr[414] = "URI Too Long";
    arr[415] = "Unsupported Media Type";
    arr[416] = "Range Not Satisfiable";
    arr[417] = "Expectation Failed";
    arr[418] = "I'm a teapot";
    arr[421] = "Misdirected Request";
    arr[422] = "Unprocessable Entity";
    arr[423] = "Locked";
    arr[424] = "Failed Dependency";
    arr[425] = "Too Early";
    arr[426] = "Upgrade Required";
    arr[428] = "Precondition Required";
    arr[429] = "Too Many Requests";
    arr[431] = "Request Header Fields Too Large";
    arr[451] = "Unavailable For Legal Reasons";
    // 5xx
    arr[500] = "Internal Server Error";
    arr[501] = "Not Implemented";
    arr[502] = "Bad Gateway";
    arr[503] = "Service Unavailable";
    arr[504] = "Gateway Timeout";
    arr[505] = "HTTP Version Not Supported";
    arr[506] = "Variant Also Negotiates";
    arr[507] = "Insufficient Storage";
    arr[508] = "Loop Detected";
    arr[510] = "Not Extended";
    arr[511] = "Network Authentication Required";
    return arr;
}();

std::string_view status_text(int code) {
    if (code >= 100 && code < 512 && !status_texts[code].empty())
        return status_texts[code];
    return "Unknown";
}

// ========== MIME 映射表 ==========
// 使用 unordered_map，但只初始化一次，查找 O(1)
static const std::unordered_map<std::string, std::string> mime_map = {
    {".aac",        "audio/aac"},
    {".abw",        "application/x-abiword"},
    {".arc",        "application/x-freearc"},
    {".avi",        "video/x-msvideo"},
    {".azw",        "application/vnd.amazon.ebook"},
    {".bin",        "application/octet-stream"},
    {".bmp",        "image/bmp"},
    {".bz",         "application/x-bzip"},
    {".bz2",        "application/x-bzip2"},
    {".csh",        "application/x-csh"},
    {".css",        "text/css"},
    {".csv",        "text/csv"},
    {".doc",        "application/msword"},
    {".docx",       "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot",        "application/vnd.ms-fontobject"},
    {".epub",       "application/epub+zip"},
    {".gif",        "image/gif"},
    {".htm",        "text/html"},
    {".html",       "text/html"},
    {".ico",        "image/vnd.microsoft.icon"},
    {".ics",        "text/calendar"},
    {".jar",        "application/java-archive"},
    {".jpeg",       "image/jpeg"},
    {".jpg",        "image/jpeg"},
    {".js",         "text/javascript"},
    {".json",       "application/json"},
    {".jsonld",     "application/ld+json"},
    {".mid",        "audio/midi"},
    {".midi",       "audio/x-midi"},
    {".mjs",        "text/javascript"},
    {".mp3",        "audio/mpeg"},
    {".mpeg",       "video/mpeg"},
    {".mpkg",       "application/vnd.apple.installer+xml"},
    {".odp",        "application/vnd.oasis.opendocument.presentation"},
    {".ods",        "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt",        "application/vnd.oasis.opendocument.text"},
    {".oga",        "audio/ogg"},
    {".ogv",        "video/ogg"},
    {".ogx",        "application/ogg"},
    {".otf",        "font/otf"},
    {".png",        "image/png"},
    {".pdf",        "application/pdf"},
    {".ppt",        "application/vnd.ms-powerpoint"},
    {".pptx",       "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar",        "application/x-rar-compressed"},
    {".rtf",        "application/rtf"},
    {".sh",         "application/x-sh"},
    {".svg",        "image/svg+xml"},
    {".swf",        "application/x-shockwave-flash"},
    {".tar",        "application/x-tar"},
    {".tif",        "image/tiff"},
    {".tiff",       "image/tiff"},
    {".ttf",        "font/ttf"},
    {".txt",        "text/plain"},
    {".vsd",        "application/vnd.visio"},
    {".wav",        "audio/wav"},
    {".weba",       "audio/webm"},
    {".webm",       "video/webm"},
    {".webp",       "image/webp"},
    {".woff",       "font/woff"},
    {".woff2",      "font/woff2"},
    {".xhtml",      "application/xhtml+xml"},
    {".xls",        "application/vnd.ms-excel"},
    {".xlsx",       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml",        "application/xml"},
    {".xul",        "application/vnd.mozilla.xul+xml"},
    {".zip",        "application/zip"},
    {".3gp",        "video/3gpp"},
    {".3g2",        "video/3gpp2"},
    {".7z",         "application/x-7z-compressed"}
};

std::string_view mime_type(std::string_view filename) {
    auto pos = filename.find_last_of('.');
    if (pos == std::string_view::npos)
        return "application/octet-stream";
    std::string ext(filename.substr(pos));
    auto it = mime_map.find(ext);
    if (it == mime_map.end())
        return "application/octet-stream";
    return it->second;
}

// ========== URL 编码 ==========
// 辅助函数：判断字符是否需要编码
static inline bool is_unreserved(char c) {
    // unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"
    return std::isalnum(static_cast<unsigned char>(c)) ||
           c == '-' || c == '.' || c == '_' || c == '~';
}

std::string url_encode(std::string_view str, bool space_to_plus) {
    std::string result;
    result.reserve(str.size() * 3); // 预留足够空间
    for (unsigned char c : str) {
        if (space_to_plus && c == ' ') {
            result += '+';
        } else if (is_unreserved(c)) {
            result += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            result.append(buf, 3);
        }
    }
    return result;
}

// 辅助函数：十六进制字符转数值
static inline char hex_to_char(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

std::string url_decode(std::string_view str, bool plus_to_space) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (plus_to_space && c == '+') {
            result += ' ';
        } else if (c == '%' && i + 2 < str.size()) {
            char h1 = hex_to_char(str[i+1]);
            char h2 = hex_to_char(str[i+2]);
            if (h1 >= 0 && h2 >= 0) {
                result += static_cast<char>((h1 << 4) | h2);
                i += 2;
            } else {
                // 非法 % 编码，原样保留
                result += c;
            }
        } else {
            result += c;
        }
    }
    return result;
}

// ========== 文件系统工具 ==========
bool is_regular_file(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool is_directory(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool is_safe_path(const std::string& path) {
    // 简单实现：规范化路径后检查是否以 ".." 开头或包含 "/.."
    // 更严格：将 path 与某个基目录拼接后规范化，检查是否在基目录内
    // 这里只做基本检查，实际使用中应与根目录拼接后检查
    if (path.empty()) return false;
    // 不允许路径中出现 ".." 导致上级目录
    if (path.find("..") != std::string::npos) return false;
    // 不允许以 '/' 开头（绝对路径）？这里根据实际需求决定
    // 若请求路径是相对根目录的，则不应以 '/' 开头，但 HTTP 请求 path 通常以 '/' 开头
    // 所以我们允许以 '/' 开头，但检查是否包含 ".." 即可
    return true;
}

bool read_file(const std::string& path, std::string* out) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
        return false;
    // 获取文件大小
    ifs.seekg(0, std::ios::end);
    size_t size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    out->resize(size);
    ifs.read(out->data(), size);
    return ifs.good();
}

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
        return false;
    ofs.write(content.data(), content.size());
    return ofs.good();
}

// ========== 字符串分割 ==========
size_t split(std::string_view str, std::string_view sep, std::vector<std::string>* out) {
    out->clear();
    if (str.empty())
        return 0;
    size_t start = 0;
    while (start < str.size()) {
        size_t pos = str.find(sep, start);
        if (pos == std::string_view::npos) {
            out->emplace_back(str.substr(start));
            break;
        }
        if (pos != start) {
            out->emplace_back(str.substr(start, pos - start));
        }
        start = pos + sep.size();
    }
    return out->size();
}

} // namespace utils