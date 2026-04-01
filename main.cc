#include "http_server.h"
#include "http_request.h"
#include "http_response.h"
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <iostream>
#include <string>

int main() {
    muduo::net::EventLoop loop;
    muduo::net::InetAddress addr(8080);
    HttpServer server(&loop, addr, "MyHttpServer");

    server.setIoThreads(4);
    server.serveStatic("/static", "./www");
    server.get("/hello", [](const HttpRequest& req, HttpResponse* resp) {
        resp->set_content("Hello World!", "text/plain");
    });
    server.get("/user/:id", [](const HttpRequest& req, HttpResponse* resp) {
        std::string_view id = req.get_path_param("id");
        resp->set_content("User ID: " + std::string(id), "text/plain");
    });
    
    server.enableAsync(8);

    std::cout << "Server listening on http://localhost:8080" << std::endl;
    server.start();
    loop.loop();

    return 0;
}