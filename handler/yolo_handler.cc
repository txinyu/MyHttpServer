#include "yolo_handler.h"
#include <fstream>

YoloHandler::YoloHandler(const std::string& modelPath, float confThreshold, float nmsThreshold)
    : confThreshold_(confThreshold), nmsThreshold_(nmsThreshold) {
    // 加载ONNX模型
    net_ = cv::dnn::readNetFromONNX(modelPath);
    // 可选：设置后端和目标
    net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    // 示例：为COCO数据集准备类别名称（仅作示例，实际应用中可从一个文件读取）
    classNames_ = {"person", "bicycle", "car", ...}; // 省略后续80个类别
}

std::vector<Detection> YoloHandler::detect(const cv::Mat& image) {
    cv::Mat blob;
    preprocess(image, blob); // 图像预处理

    net_.setInput(blob);
    std::vector<cv::Mat> outputs;
    net_.forward(outputs, net_.getUnconnectedOutLayersNames());

    std::vector<Detection> detections;
    postprocess(image, outputs, detections);
    return detections;
}

void YoloHandler::preprocess(const cv::Mat& image, cv::Mat& blob) {
    // 将图像缩放到模型所需的尺寸（例如 640x640），并进行归一化
    cv::dnn::blobFromImage(image, blob, 1.0/255.0, cv::Size(640, 640), cv::Scalar(), true, false);
}

// 后处理逻辑较为复杂，包括解析输出、应用置信度阈值和非极大值抑制(NMS)
void YoloHandler::postprocess(const cv::Mat& frame, const std::vector<cv::Mat>& outputs, std::vector<Detection>& detections) {
    // 1. 遍历所有输出，解析检测框、置信度和类别
    // 2. 应用置信度阈值 confThreshold_ 进行过滤
    // 3. 收集通过阈值的检测结果
    // 4. 应用非极大值抑制 (cv::dnn::NMSBoxes) 来过滤重叠的检测框
    // 5. 将最终结果填充到 detections 向量中
    // 此部分代码逻辑较长，可参考开源项目或网上教程实现[reference:3]
}

std::string YoloHandler::toJson(const std::vector<Detection>& detections) {
    nlohmann::json j;
    for (const auto& det : detections) {
        j["detections"].push_back({
            {"class", classNames_[det.classId]},
            {"confidence", det.confidence},
            {"x", det.box.x}, {"y", det.box.y}, {"width", det.box.width}, {"height", det.box.height}
        });
    }
    return j.dump();
}