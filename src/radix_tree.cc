#include "radix_tree.h"
#include <algorithm>
#include <cassert>

RadixTree::RadixTree() = default;
RadixTree::~RadixTree() = default;

std::vector<std::string> RadixTree::splitPath(std::string_view path)
{
    std::vector<std::string> segments;
    if (path.empty() || path == "/")
    {
        // 空路径或根路径，返回空 vector，但根路径特殊处理
        return segments;
    }
    size_t start = 0;
    // 跳过开头的 '/'
    if (path[0] == '/')
        start = 1;
    while (start < path.size())
    {
        size_t end = path.find('/', start);
        if (end == std::string_view::npos)
        {
            segments.emplace_back(path.substr(start));
            break;
        }
        else
        {
            segments.emplace_back(path.substr(start, end - start));
            start = end + 1;
        }
    }
    return segments;
}

bool RadixTree::isParamSegment(std::string_view seg, std::string *paramName)
{
    if (seg.size() > 1 && seg[0] == ':')
    {
        *paramName = seg.substr(1);
        return true;
    }
    return false;
}

bool RadixTree::isWildcardSegment(std::string_view seg)
{
    return seg == "*";
}

void RadixTree::insert(const std::string &method, const std::string &path, HttpHandler handler)
{
    auto &root = roots_[method];
    if (!root)
    {
        root = std::make_unique<Node>();
        root->path = ""; // 根节点路径为空
    }

    auto segments = splitPath(path);
    insertNode(root.get(), segments, 0, handler);
}

void RadixTree::insertNode(Node *node, const std::vector<std::string> &segments, size_t idx, HttpHandler handler)
{
    if (idx == segments.size())
    {
        // 到达路径末尾，设置处理函数
        node->handler = handler;
        return;
    }

    const std::string &seg = segments[idx];
    std::string paramName;
    bool isParam = isParamSegment(seg, &paramName);
    bool isWildcard = isWildcardSegment(seg);

    // ========== 修复：参数路由必须单独处理 ==========
    if (isParam)
    {
        auto &paramNode = node->children[':'];
        if (!paramNode)
        {
            paramNode = std::make_unique<Node>();
            paramNode->path = seg;
            paramNode->param_name = paramName;
        }
        insertNode(paramNode.get(), segments, idx + 1, handler);
        return;
    }

    // 通配符也要单独处理
    if (isWildcard)
    {
        node->children['*'] = std::make_unique<Node>();
        node->children['*']->wildcard_handler = handler;
        return;
    }

    // 查找子节点
    char firstChar = seg[0];
    auto it = node->children.find(firstChar);
    if (it != node->children.end())
    {
        Node *child = it->second.get();
        const std::string &childPath = child->path;
        // 检查公共前缀
        size_t common = 0;
        while (common < childPath.size() && common < seg.size() && childPath[common] == seg[common])
        {
            ++common;
        }
        if (common == childPath.size())
        {
            // 当前节点的路径完全匹配，继续向下
            insertNode(child, segments, idx + 1, handler);
        }
        else if (common > 0)
        {
            // 需要分割节点
            std::string remainingChild = childPath.substr(common);
            std::string remainingSeg = seg.substr(common);
            // 创建新节点作为中间节点
            auto newNode = std::make_unique<Node>();
            newNode->path = seg.substr(0, common);
            // 将原子节点移到新节点下
            newNode->children[remainingChild[0]] = std::move(it->second);
            // 替换原子节点为新节点
            node->children[firstChar] = std::move(newNode);
            Node *newChild = node->children[firstChar].get();
            // 继续插入剩余部分
            if (remainingSeg.empty())
            {
                // 当前 segment 完全匹配，处理函数应该放在新节点上
                if (isParam)
                {
                    newChild->param_handler = handler;
                    newChild->param_name = paramName;
                }
                else if (isWildcard)
                {
                    newChild->wildcard_handler = handler;
                }
                else
                {
                    newChild->handler = handler;
                }
            }
            else
            {
                // 剩余部分作为新子节点
                auto nextNode = std::make_unique<Node>();
                nextNode->path = remainingSeg;
                newChild->children[remainingSeg[0]] = std::move(nextNode);
                Node *next = newChild->children[remainingSeg[0]].get();
                insertNode(next, segments, idx + 1, handler);
            }
        }
        else
        {
            // 无公共前缀，创建新子节点
            auto newNode = std::make_unique<Node>();
            newNode->path = seg;
            node->children[firstChar] = std::move(newNode);
            Node *childNode = node->children[firstChar].get();
            insertNode(childNode, segments, idx + 1, handler);
        }
    }
    else
    {
        // 没有匹配的子节点，创建新节点
        auto newNode = std::make_unique<Node>();
        newNode->path = seg;
        node->children[firstChar] = std::move(newNode);
        Node *childNode = node->children[firstChar].get();
        insertNode(childNode, segments, idx + 1, handler);
    }
}

bool RadixTree::route(const std::string &method, const std::string &path,
                      HttpHandler *handler, std::unordered_map<std::string, std::string> *params) const
{
    auto it = roots_.find(method);
    if (it == roots_.end())
    {
        return false;
    }
    const Node *root = it->second.get();
    if (!root)
        return false;

    auto segments = splitPath(path);
    return routeNode(root, segments, 0, handler, params);
}

bool RadixTree::routeNode(const Node *node, const std::vector<std::string> &segments, size_t idx,
                          HttpHandler *handler, std::unordered_map<std::string, std::string> *params) const
{
    if (idx == segments.size())
    {
        // 到达路径末尾
        if (node->handler)
        {
            *handler = node->handler;
            return true;
        }
        // 可能还有通配符匹配？通配符应该至少匹配一段，所以路径末尾没有通配符
        return false;
    }

    const std::string &seg = segments[idx];
    // 优先精确匹配
    if (!seg.empty())
    {
        char firstChar = seg[0];
        auto it = node->children.find(firstChar);
        if (it != node->children.end())
        {
            const Node *child = it->second.get();
            const std::string &childPath = child->path;
            // 检查当前段是否匹配子节点路径
            if (childPath.size() <= seg.size() && seg.compare(0, childPath.size(), childPath) == 0)
            {
                // 如果子节点路径完全匹配，继续
                if (routeNode(child, segments, idx + 1, handler, params))
                {
                    return true;
                }
            }
        }
    }

    // 尝试参数匹配（:param）
    // 参数节点通常以 ':' 开头，但子节点映射键是 ':' 字符
    auto paramIt = node->children.find(':');
    if (paramIt != node->children.end())
    {
        const Node *paramNode = paramIt->second.get();
        // 参数节点匹配一个路径段
        std::string paramValue = seg; // 整个段作为参数值
        if (paramNode->param_name.empty())
        {
            // 按理说不应该为空
            return false;
        }
        if (params)
        {
            (*params)[paramNode->param_name] = paramValue;
        }
        if (routeNode(paramNode, segments, idx + 1, handler, params))
        {
            return true;
        }
        // 如果参数节点后继续匹配失败，需要回滚参数（但我们的设计是参数节点后可能还有子节点，递归已经处理）
        // 如果递归失败，我们需要清除参数？但递归会覆盖，我们无需显式清除。
    }

    // 尝试通配符匹配（*）
    auto wildIt = node->children.find('*');
    if (wildIt != node->children.end())
    {
        const Node *wildNode = wildIt->second.get();
        // 通配符匹配剩余所有路径段
        std::string wildValue;
        for (size_t i = idx; i < segments.size(); ++i)
        {
            if (!wildValue.empty())
                wildValue += '/';
            wildValue += segments[i];
        }
        if (params)
        {
            // 通配符参数名可能是 "filepath" 或其他，从节点获取
            // 这里简化：通配符节点名称通常是 "*"，我们设置一个固定参数名
            (*params)["*"] = wildValue;
        }
        // 通配符节点应该有一个处理函数
        if (wildNode->wildcard_handler)
        {
            *handler = wildNode->wildcard_handler;
            return true;
        }
    }

    return false;
}