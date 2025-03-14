// include/detection/fall_detector.hpp
#pragma once

#include <vector>
#include <chrono>
#include <map>
#include <opencv2/opencv.hpp>
#include "detection/human_detector.hpp"

namespace hms {

struct FallEvent {
    int personId;
    std::chrono::steady_clock::time_point startTime;
    bool alerted;
    cv::Mat frameSnapshot;
    cv::Rect position;
};

class FallDetector {
public:
    FallDetector(int fallDurationThresholdSec = 10);
    ~FallDetector();
    
    void analyze(const std::vector<DetectedPerson>& persons, const cv::Mat& frame);
    std::vector<FallEvent> getActiveFallEvents() const;
    std::vector<int> getNewAlerts();
    
private:
    std::map<int, FallEvent> m_fallEvents;
    std::vector<int> m_newAlerts;
    int m_fallDurationThreshold;  // Duration in seconds to trigger alert
    
    bool isPoseIndicatingFall(const DetectedPerson& person);
    bool isPersonOnGround(const DetectedPerson& person);
};

} // namespace hms
