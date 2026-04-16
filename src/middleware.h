#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include <functional>
#include <vector>
#include <memory>

class HttpRequest;
class HttpResponse;

// 前向声明：Next 是一个可调用对象，用于调用链中的下一个中间件
using Next = std::function<void()>;

// 中间件函数签名：
//   - 参数：请求对象、响应对象、下一个中间件的回调
//   - 返回值：bool，true 表示继续执行下一个中间件，false 表示终止链（响应已由中间件处理）
using Middleware = std::function<bool(HttpRequest&, HttpResponse&, Next)>;

// 中间件链：管理并执行一组中间件
class MiddlewareChain {
public:
    MiddlewareChain() = default;
    ~MiddlewareChain() = default;

    // 禁止拷贝，允许移动
    MiddlewareChain(const MiddlewareChain&) = delete;
    MiddlewareChain& operator=(const MiddlewareChain&) = delete;
    MiddlewareChain(MiddlewareChain&&) = default;
    MiddlewareChain& operator=(MiddlewareChain&&) = default;

    // 添加一个中间件到链末尾
    void add(Middleware mw);

    // 运行中间件链，从第一个开始依次执行
    // 参数 req, resp 会被传递给每个中间件
    // 当所有中间件都返回 true 且未提前终止时，会调用 done() 回调
    void run(HttpRequest& req, HttpResponse& resp, std::function<void()> done);

    // 清空所有中间件
    void clear();

    // 获取中间件数量
    size_t size() const { return middlewares_.size(); }

private:
    std::vector<Middleware> middlewares_;
};

#endif // MIDDLEWARE_H