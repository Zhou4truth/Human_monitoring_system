#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

namespace hms {

// Structure to represent a detected person
struct DetectedPerson {
    int id;
    cv::Rect boundingBox;
    float confidence;
    std::vector<cv::Point> keypoints;
    cv::Mat appearance;
    bool isFallen;
    cv::Scalar color;
    std::string name;
    
    DetectedPerson() : id(-1), confidence(0.0f), isFallen(false), color(0, 255, 0) {}
};

// Class for human detection using YOLOv8
class HumanDetector {
public:
    HumanDetector(const std::string& modelPath, float confThreshold = 0.5f, 
                 float nmsThreshold = 0.45f, int inputWidth = 640, int inputHeight = 640)
        : m_modelPath(modelPath), m_confThreshold(confThreshold), 
          m_nmsThreshold(nmsThreshold), m_inputWidth(inputWidth), 
          m_inputHeight(inputHeight), m_initialized(false) {}
    
    ~HumanDetector() {}
    
    bool initialize() {
        try {
            // Load the network
            m_net = cv::dnn::readNet(m_modelPath);
            
            // Set backend and target
            m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            
            // Get output layers
            std::vector<int> outLayers = m_net.getUnconnectedOutLayers();
            std::vector<std::string> layersNames = m_net.getLayerNames();
            m_outputLayerNames.resize(outLayers.size());
            
            for (size_t i = 0; i < outLayers.size(); ++i) {
                m_outputLayerNames[i] = layersNames[outLayers[i] - 1];
            }
            
            m_initialized = true;
            return true;
        } catch (const cv::Exception& e) {
            std::cerr << "Error initializing YOLO model: " << e.what() << std::endl;
            return false;
        }
    }
    
    void preprocess(const cv::Mat& frame, cv::Mat& blob) {
        // YOLOv8 uses 640x640 input size
        cv::dnn::blobFromImage(frame, blob, 1/255.0, cv::Size(m_inputWidth, m_inputHeight), cv::Scalar(), true, false);
    }
    
    std::vector<DetectedPerson> detectPersons(const cv::Mat& frame) {
        if (!m_initialized && !initialize()) {
            return {};
        }
        
        cv::Mat blob;
        preprocess(frame, blob);
        
        m_net.setInput(blob);
        
        std::vector<cv::Mat> outputs;
        m_net.forward(outputs, m_outputLayerNames);
        
        return postprocess(frame, outputs);
    }
    
    std::vector<DetectedPerson> postprocess(const cv::Mat& frame, const std::vector<cv::Mat>& outputs) {
        std::vector<DetectedPerson> persons;
        
        // YOLOv8 has a different output format than YOLOv5
        // Process the detection results
        const int personClassId = 0; // In COCO dataset, person is class 0
        
        for (size_t i = 0; i < outputs.size(); ++i) {
            float* data = (float*)outputs[i].data;
            for (int j = 0; j < outputs[i].rows; ++j, data += outputs[i].cols) {
                cv::Mat scores = outputs[i].row(j).colRange(5, outputs[i].cols);
                cv::Point classIdPoint;
                double confidence;
                
                // Get the value and location of the maximum score
                cv::minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
                int classId = classIdPoint.x;
                
                if (classId == personClassId && confidence > m_confThreshold) {
                    DetectedPerson person;
                    person.confidence = static_cast<float>(confidence);
                    
                    // Center, width, height
                    float cx = data[0];
                    float cy = data[1];
                    float w = data[2];
                    float h = data[3];
                    
                    // Calculate top-left corner
                    int left = static_cast<int>((cx - w/2) * frame.cols);
                    int top = static_cast<int>((cy - h/2) * frame.rows);
                    
                    person.boundingBox = cv::Rect(left, top, 
                                                 static_cast<int>(w * frame.cols), 
                                                 static_cast<int>(h * frame.rows));
                    
                    // Ensure the bounding box is within the frame
                    person.boundingBox &= cv::Rect(0, 0, frame.cols, frame.rows);
                    
                    // Extract appearance feature for tracking
                    if (person.boundingBox.width > 0 && person.boundingBox.height > 0) {
                        person.appearance = frame(person.boundingBox).clone();
                        persons.push_back(person);
                    }
                }
            }
        }
        
        // Apply non-maximum suppression
        std::vector<int> indices;
        std::vector<cv::Rect> boxes;
        std::vector<float> scores;
        
        for (const auto& person : persons) {
            boxes.push_back(person.boundingBox);
            scores.push_back(person.confidence);
        }
        
        cv::dnn::NMSBoxes(boxes, scores, m_confThreshold, m_nmsThreshold, indices);
        
        std::vector<DetectedPerson> filteredPersons;
        for (size_t i = 0; i < indices.size(); ++i) {
            filteredPersons.push_back(persons[indices[i]]);
        }
        
        return filteredPersons;
    }
    
private:
    std::string m_modelPath;
    float m_confThreshold;
    float m_nmsThreshold;
    int m_inputWidth;
    int m_inputHeight;
    bool m_initialized;
    cv::dnn::Net m_net;
    std::vector<std::string> m_outputLayerNames;
};

// Class for tracking detected persons across frames
class PersonTracker {
public:
    PersonTracker() : m_nextId(0) {}
    
    ~PersonTracker() {}
    
    void update(std::vector<DetectedPerson>& detections, const cv::Mat& frame) {
        if (m_trackedPersons.empty()) {
            // First frame, assign IDs to all detections
            for (auto& detection : detections) {
                detection.id = m_nextId++;
                m_trackedPersons.push_back(detection);
            }
            return;
        }
        
        // Match detections with existing tracks
        std::vector<int> assignedTracks(m_trackedPersons.size(), -1);
        std::vector<bool> assignedDetections(detections.size(), false);
        
        // For each detection, find the best matching track
        for (size_t i = 0; i < detections.size(); ++i) {
            int bestTrackIdx = matchDetection(detections[i].boundingBox, m_trackedPersons);
            
            if (bestTrackIdx >= 0) {
                assignedTracks[bestTrackIdx] = i;
                assignedDetections[i] = true;
                
                // Update the track with new detection
                detections[i].id = m_trackedPersons[bestTrackIdx].id;
            }
        }
        
        // Update tracked persons list
        std::vector<DetectedPerson> newTracks;
        
        // Add matched tracks
        for (size_t i = 0; i < m_trackedPersons.size(); ++i) {
            if (assignedTracks[i] >= 0) {
                newTracks.push_back(detections[assignedTracks[i]]);
            }
        }
        
        // Add new tracks for unmatched detections
        for (size_t i = 0; i < detections.size(); ++i) {
            if (!assignedDetections[i]) {
                detections[i].id = m_nextId++;
                newTracks.push_back(detections[i]);
            }
        }
        
        m_trackedPersons = newTracks;
    }
    
    int matchDetection(const cv::Rect& detection, const std::vector<DetectedPerson>& existingTracks) {
        double maxIoU = 0.3; // Minimum IoU threshold for a match
        int bestMatch = -1;
        
        for (size_t i = 0; i < existingTracks.size(); ++i) {
            double iou = calculateIoU(detection, existingTracks[i].boundingBox);
            
            if (iou > maxIoU) {
                maxIoU = iou;
                bestMatch = i;
            }
        }
        
        return bestMatch;
    }
    
    double calculateIoU(const cv::Rect& box1, const cv::Rect& box2) {
        int x1 = std::max(box1.x, box2.x);
        int y1 = std::max(box1.y, box2.y);
        int x2 = std::min(box1.x + box1.width, box2.x + box2.width);
        int y2 = std::min(box1.y + box1.height, box2.y + box2.height);
        
        if (x2 < x1 || y2 < y1) {
            return 0.0;
        }
        
        double intersectionArea = (x2 - x1) * (y2 - y1);
        double box1Area = box1.width * box1.height;
        double box2Area = box2.width * box2.height;
        
        return intersectionArea / (box1Area + box2Area - intersectionArea);
    }
    
    cv::Scalar generateUniqueColor(int id) {
        // Generate a unique color based on ID
        int hue = (id * 30) % 180;
        return cv::Scalar(hue, 255, 255); // HSV color
    }
    
    const std::vector<DetectedPerson>& getTrackedPersons() const {
        return m_trackedPersons;
    }
    
private:
    std::vector<DetectedPerson> m_trackedPersons;
    int m_nextId;
};

} // namespace hms
