#ifndef RADIX_TREE_H
#define RADIX_TREE_H

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>

// 前向声明
class HttpRequest;
class HttpResponse;

using HttpHandler = std::function<void(const HttpRequest&, HttpResponse*)>;

class RadixTree {
public:
    RadixTree();
    ~RadixTree();

    // 插入路由，method 为 HTTP 方法字符串（如 "GET"），path 为路由路径（如 "/user/:id"）
    void insert(const std::string& method, const std::string& path, HttpHandler handler);

    // 路由匹配，若找到处理函数，返回 true，并通过 handler 返回函数，通过 params 返回路径参数
    bool route(const std::string& method, const std::string& path,
               HttpHandler* handler, std::unordered_map<std::string, std::string>* params) const;

private:
    struct Node {
        std::string path;   // 当前节点存储的路径片段
        std::unordered_map<char, std::unique_ptr<Node>> children;
        HttpHandler handler;          // 精确匹配的处理函数
        HttpHandler param_handler;    // 参数匹配的处理函数（:param）
        HttpHandler wildcard_handler; // 通配符匹配的处理函数（*）
        std::string param_name;       // 参数名（如 "id"）
        std::string wildcard_name;    // 通配符参数名（如 "filepath"）

        Node() = default;
    };

    // 将路径按 '/' 分割成段，返回段列表（空路径或根路径返回空 vector）
    static std::vector<std::string> splitPath(std::string_view path);

    // 判断段是否为参数段（如 :id），若是则返回 true 并设置 paramName
    static bool isParamSegment(std::string_view seg, std::string* paramName);

    // 判断段是否为通配符段（如 *filepath）
    static bool isWildcardSegment(std::string_view seg);

    // 插入节点递归辅助函数
    void insertNode(Node* node, const std::vector<std::string>& segments, size_t idx, HttpHandler handler);

    // 路由匹配递归辅助函数
    bool routeNode(const Node* node, const std::vector<std::string>& segments, size_t idx,
                   HttpHandler* handler, std::unordered_map<std::string, std::string>* params) const;

    // 根节点按 HTTP 方法存储，每种方法一棵树
    std::unordered_map<std::string, std::unique_ptr<Node>> roots_;
};

#endif // RADIX_TREE_H