#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cctype>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

atomic<int> g_success{0};
atomic<int> g_failed{0};

static const char* REQUEST =
    "GET /hello HTTP/1.1\r\n"
    "Host: 127.0.0.1:8080\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";

bool sendAll(int fd, const char* data, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }

    return true;
}

string toLower(string s) {
    for (char& c : s) {
        c = static_cast<char>(tolower(c));
    }
    return s;
}

int parseContentLength(const string& header) {
    string lower = toLower(header);
    string key = "content-length:";

    size_t pos = lower.find(key);
    if (pos == string::npos) {
        return -1;
    }

    pos += key.size();

    while (pos < header.size() && isspace(header[pos])) {
        ++pos;
    }

    int len = 0;
    while (pos < header.size() && isdigit(header[pos])) {
        len = len * 10 + (header[pos] - '0');
        ++pos;
    }

    return len;
}

bool recvHttpResponse(int fd) {
    string response;
    char buf[4096];

    size_t header_end = string::npos;
    int content_length = -1;

    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            return false;
        }

        response.append(buf, n);

        if (header_end == string::npos) {
            header_end = response.find("\r\n\r\n");

            if (header_end != string::npos) {
                string header = response.substr(0, header_end + 4);
                content_length = parseContentLength(header);

                if (content_length < 0) {
                    return false;
                }
            }
        }

        if (header_end != string::npos) {
            size_t full_size = header_end + 4 + content_length;

            if (response.size() >= full_size) {
                return true;
            }
        }
    }
}

void worker(int request_num, vector<double>& latencies_us) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return;
    }

    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return;
    }

    latencies_us.reserve(request_num);

    for (int i = 0; i < request_num; ++i) {
        auto t1 = chrono::steady_clock::now();

        bool ok = sendAll(fd, REQUEST, strlen(REQUEST));
        if (!ok) {
            g_failed++;
            break;
        }

        ok = recvHttpResponse(fd);
        if (!ok) {
            g_failed++;
            break;
        }

        auto t2 = chrono::steady_clock::now();

        double us = chrono::duration<double, micro>(t2 - t1).count();

        latencies_us.push_back(us);
        g_success++;
    }

    close(fd);
}

double percentile(const vector<double>& data, double p) {
    if (data.empty()) {
        return 0.0;
    }

    size_t idx = static_cast<size_t>(data.size() * p);
    if (idx >= data.size()) {
        idx = data.size() - 1;
    }

    return data[idx];
}

int main() {
    int threads = 16;
    int total = 100000;

    vector<thread> ts;
    vector<vector<double>> all_latencies(threads);

    auto start = chrono::steady_clock::now();

    for (int i = 0; i < threads; ++i) {
        int request_num = total / threads;

        if (i < total % threads) {
            request_num++;
        }

        ts.emplace_back(worker, request_num, ref(all_latencies[i]));
    }

    for (auto& t : ts) {
        t.join();
    }

    auto end = chrono::steady_clock::now();

    vector<double> merged;
    for (auto& v : all_latencies) {
        merged.insert(merged.end(), v.begin(), v.end());
    }

    sort(merged.begin(), merged.end());

    double sec = chrono::duration<double>(end - start).count();
    double qps = g_success.load() / sec;

    double sum = 0.0;
    for (double x : merged) {
        sum += x;
    }

    double avg = merged.empty() ? 0.0 : sum / merged.size();

    cout << "\n===== 长连接单请求延迟压测结果 =====\n";
    cout << "线程数: " << threads << endl;
    cout << "目标请求数: " << total << endl;
    cout << "成功请求数: " << g_success.load() << endl;
    cout << "失败请求数: " << g_failed.load() << endl;
    cout << "总耗时: " << sec << " s" << endl;
    cout << "QPS: " << static_cast<int>(qps) << endl;

    cout << "\n===== 单请求延迟统计 =====\n";
    cout << "平均延迟: " << avg << " us" << endl;
    cout << "P50 延迟: " << percentile(merged, 0.50) << " us" << endl;
    cout << "P90 延迟: " << percentile(merged, 0.90) << " us" << endl;
    cout << "P99 延迟: " << percentile(merged, 0.99) << " us" << endl;

    if (!merged.empty()) {
        cout << "最小延迟: " << merged.front() << " us" << endl;
        cout << "最大延迟: " << merged.back() << " us" << endl;
    }

    return 0;
}