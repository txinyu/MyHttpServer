#ifndef YOLO_HANDLER_H
#define YOLO_HANDLER_H

#include <string>
#include <vector>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <nlohmann/json.hpp> // 强烈推荐使用 JSON 库

// 用于存储检测结果的简单结构体
struct Detection {
    int classId;
    float confidence;
    cv::Rect box;
};

class YoloHandler {
public:
    // 构造函数，加载模型
    YoloHandler(const std::string& modelPath, 
                float confThreshold = 0.5, 
                float nmsThreshold = 0.4);

    // 执行检测的核心方法
    std::vector<Detection> detect(const cv::Mat& image);

    // 将检测结果转换为 JSON 字符串
    std::string toJson(const std::vector<Detection>& detections);

private:
    cv::dnn::Net net_;
    float confThreshold_;
    float nmsThreshold_;
    std::vector<std::string> classNames_; // 可以从文件中加载，此处简化

    void preprocess(const cv::Mat& image, cv::Mat& blob);
    void postprocess(const cv::Mat& frame, const std::vector<cv::Mat>& outputs, std::vector<Detection>& detections);
};
#endif // YOLO_HANDLER_H