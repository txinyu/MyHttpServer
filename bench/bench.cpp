#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

const char* REQUEST =
    "GET /hello HTTP/1.1\r\n"
    "Host: 127.0.0.1:8080\r\n"
    "Connection: close\r\n\r\n";

atomic<int> g_success{0};

void worker(int request_num) {
    for (int i = 0; i < request_num; ++i) {
        // 1. 创建 socket
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) continue;

        // 2. 连接 8080
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8080);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd);
            continue;
        }

        // 3. 发送请求
        send(fd, REQUEST, strlen(REQUEST), MSG_NOSIGNAL);

        // 4. 读取响应（不处理内容）
        char buf[2048];
        recv(fd, buf, sizeof(buf), 0);

        close(fd);
        g_success++;
    }
}

int main() {
    // 压测配置
    int thread_num = 16;    // 16 并发
    int total_request = 20000; // 总请求数

    printf("压测开始：http://127.0.0.1:8080/hello\n");
    printf("并发：%d，总请求：%d\n\n", thread_num, total_request);

    auto start = chrono::steady_clock::now();

    vector<thread> threads;
    int per_thread = total_request / thread_num;

    for (int i = 0; i < thread_num; ++i) {
        threads.emplace_back(worker, per_thread);
    }

    for (auto& t : threads) t.join();

    auto end = chrono::steady_clock::now();
    double sec = chrono::duration<double>(end - start).count();
    double qps = g_success / sec;

    printf("===== 压测结果 =====\n");
    printf("成功请求：%d\n", g_success.load());
    printf("总耗时：%.2f s\n", sec);
    printf("QPS：%.0f\n", qps);

    return 0;
}