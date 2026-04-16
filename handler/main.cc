// 在 main.cc 或服务器初始化代码中
#include "my_http/http_server.h"
#include "yolo_handler.h"

int main() {
    // ... 创建 EventLoop 和 HttpServer ...
    my_http::HttpServer server(&loop, listenAddr, "YoloServer");

    // 创建 YOLO Handler 实例，并加载模型文件
    YoloHandler detector("yolov5s.onnx"); // 请替换为你的模型文件路径

    // 注册 POST 路径 /detect
    server.registerHandler("/detect", [&detector](const HttpRequest& req, HttpResponse& resp) {
        // 1. 从 req 的 body 中提取图像数据（需解析 multipart/form-data 或 base64 格式）
        cv::Mat image = extractImageFromRequest(req); 
        if (image.empty()) {
            resp.setStatusCode(HttpResponse::k400BadRequest);
            resp.setBody("{\"error\": \"No image data found\"}");
            return;
        }

        // 2. 调用检测器进行推理
        auto detections = detector.detect(image);
        
        // 3. 将结果转为 JSON 并返回
        resp.setStatusCode(HttpResponse::k200Ok);
        resp.setContentType("application/json");
        resp.setBody(detector.toJson(detections));
    });

    server.start();
    loop.loop();
    return 0;
}