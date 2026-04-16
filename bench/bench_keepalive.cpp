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

atomic<int> g_success{0};

void worker(int request_num) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return;
    }

    for (int i = 0; i < request_num; ++i) {
        const char* req =
            "GET /hello HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Connection: keep-alive\r\n\r\n";

        send(fd, req, strlen(req), MSG_NOSIGNAL);

        char buf[4096];
        recv(fd, buf, sizeof(buf), 0);
        g_success++;
    }

    close(fd);
}

int main() {
    int threads = 16;
    int total = 100000;

    auto start = chrono::steady_clock::now();

    vector<thread> ts;
    int per = total / threads;
    for (int i = 0; i < threads; ++i)
        ts.emplace_back(worker, per);
    for (auto& t : ts) t.join();

    auto end = chrono::steady_clock::now();
    double sec = chrono::duration<double>(end - start).count();
    double qps = g_success / sec;

    cout << "\n===== 长连接压测结果 =====\n";
    cout << "成功: " << g_success << endl;
    cout << "耗时: " << sec << "s" << endl;
    cout << "QPS:  " << (int)qps << endl;
}