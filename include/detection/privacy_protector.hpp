// include/detection/privacy_protector.hpp
#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <string>
#include <vector>
#include "detection/human_detector.hpp"

namespace hms {

class PrivacyProtector {
public:
    PrivacyProtector(const std::string& nudityModelPath);
    ~PrivacyProtector();
    
    bool initialize();
    cv::Mat applyPrivacyFilters(const cv::Mat& frame, const std::vector<DetectedPerson>& persons);
    
private:
    cv::dnn::Net m_nudityNet;
    std::string m_modelPath;
    bool m_initialized;
    float m_confidenceThreshold;
    
    bool detectNudity(const cv::Mat& personROI);
    cv::Mat blurSensitiveAreas(const cv::Mat& personROI);
    std::vector<cv::Rect> detectSensitiveAreas(const cv::Mat& personROI);
};

} // namespace hms
