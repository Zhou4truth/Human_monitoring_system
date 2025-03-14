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

// src/detection/fall_detector.cpp
#include "detection/fall_detector.hpp"

namespace hms {

FallDetector::FallDetector(int fallDurationThresholdSec)
    : m_fallDurationThreshold(fallDurationThresholdSec) {
}

FallDetector::~FallDetector() {
}

void FallDetector::analyze(const std::vector<DetectedPerson>& persons, const cv::Mat& frame) {
    m_newAlerts.clear();
    
    // Process each detected person
    std::vector<int> activePeople;
    for (const auto& person : persons) {
        activePeople.push_back(person.id);
        
        // Check if person is fallen
        if (isPersonOnGround(person)) {
            auto now = std::chrono::steady_clock::now();
            
            // If this is a new fall event
            if (m_fallEvents.find(person.id) == m_fallEvents.end()) {
                FallEvent event;
                event.personId = person.id;
                event.startTime = now;
                event.alerted = false;
                event.position = person.boundingBox;
                
                // Save a snapshot of the frame
                cv::Rect roi = person.boundingBox;
                
                // Ensure ROI is within frame boundaries
                roi.x = std::max(0, roi.x);
                roi.y = std::max(0, roi.y);
                roi.width = std::min(roi.width, frame.cols - roi.x);
                roi.height = std::min(roi.height, frame.rows - roi.y);
                
                if (roi.width > 0 && roi.height > 0) {
                    event.frameSnapshot = frame(roi).clone();
                }
                
                m_fallEvents[person.id] = event;
            } else {
                // Update existing fall event
                auto& event = m_fallEvents[person.id];
                event.position = person.boundingBox;
                
                // Check if we need to trigger an alert
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    now - event.startTime).count();
                
                if (duration >= m_fallDurationThreshold && !event.alerted) {
                    event.alerted = true;
                    m_newAlerts.push_back(person.id);
                }
            }
        } else {
            // Person is not fallen, remove from fall events if present
            if (m_fallEvents.find(person.id) != m_fallEvents.end()) {
                m_fallEvents.erase(person.id);
            }
        }
    }
    
    // Remove fall events for people no longer detected
    for (auto it = m_fallEvents.begin(); it != m_fallEvents.end();) {
        bool personFound = std::find(activePeople.begin(), activePeople.end(), 
                                    it->first) != activePeople.end();
        if (!personFound) {
            it = m_fallEvents.erase(it);
        } else {
            ++it;
        }
    }
}

bool FallDetector::isPoseIndicatingFall(const DetectedPerson& person) {
    // In a real implementation, this would analyze the pose keypoints
    // to determine if the person's posture indicates a fall
    
    // For now, using a simplified approach based on bounding box aspect ratio
    // In real implementation, you would use proper pose estimation
    return false;  // Placeholder
}

bool FallDetector::isPersonOnGround(const DetectedPerson& person) {
    // A simple heuristic: if the bounding box width is greater than height,
    // the person might be lying on the ground
    float aspectRatio = static_cast<float>(person.boundingBox.width) / 
                        static_cast<float>(person.boundingBox.height);
    
    // More sophisticated implementation would use pose estimation to check body orientation
    return aspectRatio > 1.5f;  // A threshold for detecting horizontal posture
}

std::vector<FallEvent> FallDetector::getActiveFallEvents() const {
    std::vector<FallEvent> events;
    for (const auto& pair : m_fallEvents) {
        events.push_back(pair.second);
    }
    return events;
}

std::vector<int> FallDetector::getNewAlerts() {
    return m_newAlerts;
}

} // namespace hms
