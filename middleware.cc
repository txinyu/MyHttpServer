// middleware.cc
#include "middleware.h"
#include "http_request.h"
#include "http_response.h"
#include <functional>
#include <memory>
#include <atomic>

void MiddlewareChain::add(Middleware mw) {
    middlewares_.push_back(std::move(mw));
}

void MiddlewareChain::run(HttpRequest& req, HttpResponse& resp, std::function<void()> done) {
    // 使用 shared_ptr 管理状态，支持异步调用
    struct State  : std::enable_shared_from_this<State> {
        size_t index = 0;
        std::function<void()> done;
        bool executed = false;  // 防止 done 被多次调用
        MiddlewareChain* chain;
        HttpRequest* req;
        HttpResponse* resp;

        void next() {
            if (executed) return; // 防止多次调用 next
            if (index >= chain->middlewares_.size()) {
                if (done && !executed) {
                    executed = true;
                    done();
                }
                return;
            }
            auto mw = chain->middlewares_[index++];
            // 注意：此处捕获 shared_ptr 保持状态存活
            auto self = shared_from_this(); // 需要 enable_shared_from_this
            try {
                bool cont = mw(*req, *resp, [self]() {
                    self->next();
                });
                if (!cont) {
                    // 中间件决定终止链，不调用 done
                    executed = true; // 标记已处理，避免 done 被调用
                    return;
                }
            } catch (...) {
                // 异常时终止链，可以记录日志或返回 500
                if (done && !executed) {
                    executed = true;
                    // 可选：设置响应状态码
                    resp->set_status_code(500);
                    done();
                }
            }
        }
    };

    auto state = std::make_shared<State>();
    state->index = 0;
    state->done = std::move(done);
    state->executed = false;
    state->chain = this;
    state->req = &req;
    state->resp = &resp;

    state->next(); // 开始执行
}