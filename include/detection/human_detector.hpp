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

void HumanDetector::preprocess(const cv::Mat& frame, cv::Mat& blob) {
    // YOLOv8 uses 640x640 input size
    cv::dnn::blobFromImage(frame, blob, 1/255.0, cv::Size(640, 640), cv::Scalar(), true, false);
}

std::vector<DetectedPerson> HumanDetector::detectPersons(const cv::Mat& frame) {
    if (!m_initialized) {
        if (!initialize()) {
            return {};
        }
    }
    
    cv::Mat blob;
    preprocess(frame, blob);
    
    m_net.setInput(blob);
    
    std::vector<cv::Mat> outputs;
    m_net.forward(outputs, m_outputLayerNames);
    
    return postprocess(frame, outputs);
}

std::vector<DetectedPerson> HumanDetector::postprocess(const cv::Mat& frame, 
                                                     const std::vector<cv::Mat>& outputs) {
    std::vector<DetectedPerson> persons;
    
    // YOLOv8 output format processing
    // Extract the output tensor containing detection results
    const auto& output = outputs[0]; // For YOLOv8, we typically have a single output tensor
    
    // Process detections
    const int numDetections = output.size[1];
    const int numAttributes = output.size[2];
    
    // Get scaling factors
    float xScale = static_cast<float>(frame.cols) / 640.0f;
    float yScale = static_cast<float>(frame.rows) / 640.0f;
    
    // Process each detection
    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    
    // YOLO output: [batch_id, x, y, w, h, confidence, class_scores...]
    for (int i = 0; i < numDetections; i++) {
        // Get the row for the current detection
        cv::Mat detection = output.row(i);
        
        // Get confidence (objectness score)
        float confidence = detection.at<float>(4);
        
        if (confidence > m_confThreshold) {
            // Get class scores starting from index 5
            cv::Mat scores = detection.colRange(5, numAttributes);
            
            // Find the highest class score and its index
            cv::Point classIdPoint;
            double maxScore;
            minMaxLoc(scores, nullptr, &maxScore, nullptr, &classIdPoint);
            
            int classId = classIdPoint.x;
            float classScore = static_cast<float>(maxScore);
            
            // Person class ID is 0 in COCO dataset
            if (classId == 0) {  // Filter for person class only
                float finalConfidence = confidence * classScore;
                
                if (finalConfidence > m_confThreshold) {
                    // Get bounding box coordinates
                    float centerX = detection.at<float>(0) * xScale;
                    float centerY = detection.at<float>(1) * yScale;
                    float width = detection.at<float>(2) * xScale;
                    float height = detection.at<float>(3) * yScale;
                    
                    int left = static_cast<int>(centerX - width / 2);
                    int top = static_cast<int>(centerY - height / 2);
                    
                    classIds.push_back(classId);
                    confidences.push_back(finalConfidence);
                    boxes.push_back(cv::Rect(left, top, static_cast<int>(width), static_cast<int>(height)));
                }
            }
        }
    }
    
    // Apply non-maximum suppression
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, m_confThreshold, m_nmsThreshold, indices);
    
    // Create DetectedPerson objects
    for (size_t i = 0; i < indices.size(); ++i) {
        int idx = indices[i];
        
        DetectedPerson person;
        person.boundingBox = boxes[idx];
        person.confidence = confidences[idx];
        person.id = -1;  // Will be assigned by tracker
        person.isFallen = false;  // Will be determined by fall detector
        person.color = cv::Scalar(0, 0, 255);  // Default color (will be assigned by tracker)
        
        // For now, set empty pose. This will be filled by a separate pose estimator
        person.pose = std::vector<cv::Point>();
        
        persons.push_back(person);
    }
    
    return persons;
}

// PersonTracker implementation
PersonTracker::PersonTracker() : m_nextId(0) {
}

PersonTracker::~PersonTracker() {
}

void PersonTracker::update(std::vector<DetectedPerson>& detections, const cv::Mat& frame) {
    if (m_trackedPersons.empty()) {
        // First frame, assign IDs to all detections
        for (auto& detection : detections) {
            detection.id = m_nextId++;
            detection.color = generateUniqueColor(detection.id);
            m_trackedPersons.push_back(detection);
        }
    } else {
        // Create a copy of currently tracked persons
        std::vector<DetectedPerson> previousTracked = m_trackedPersons;
        m_trackedPersons.clear();
        
        // For each detection, find the best match in previous tracks
        for (auto& detection : detections) {
            int matchId = matchDetection(detection.boundingBox, previousTracked);
            
            if (matchId >= 0) {
                // Update existing track
                detection.id = previousTracked[matchId].id;
                detection.color = previousTracked[matchId].color;
                detection.name = previousTracked[matchId].name;
                
                // Remove the matched track from previous
                previousTracked.erase(previousTracked.begin() + matchId);
            } else {
                // New track
                detection.id = m_nextId++;
                detection.color = generateUniqueColor(detection.id);
            }
            
            m_trackedPersons.push_back(detection);
        }
    }
}

int PersonTracker::matchDetection(const cv::Rect& detection, 
                                 const std::vector<DetectedPerson>& existingTracks) {
    // Find the best match based on IoU (Intersection over Union)
    float bestIoU = 0.3f;  // Minimum IoU threshold
    int bestIdx = -1;
    
    for (size_t i = 0; i < existingTracks.size(); ++i) {
        const cv::Rect& trackBox = existingTracks[i].boundingBox;
        
        // Calculate intersection
        cv::Rect intersection = detection & trackBox;
        float intersectionArea = intersection.width * intersection.height;
        
        // Calculate union
        float detectionArea = detection.width * detection.height;
        float trackArea = trackBox.width * trackBox.height;
        float unionArea = detectionArea + trackArea - intersectionArea;
        
        // Calculate IoU
        float iou = intersectionArea / unionArea;
        
        if (iou > bestIoU) {
            bestIoU = iou;
            bestIdx = static_cast<int>(i);
        }
    }
    
    return bestIdx;
}

cv::Scalar PersonTracker::generateUniqueColor(int id) {
    // Generate a visually distinct color for each person
    static std::vector<cv::Scalar> colorPalette = {
        cv::Scalar(255, 0, 0),    // Blue
        cv::Scalar(0, 255, 0),    // Green
        cv::Scalar(0, 0, 255),    // Red
        cv::Scalar(255, 255, 0),  // Cyan
        cv::Scalar(255, 0, 255),  // Magenta
        cv::Scalar(0, 255, 255),  // Yellow
        cv::Scalar(255, 128, 0),  // Light Blue
        cv::Scalar(128, 255, 0),  // Light Green
        cv::Scalar(128, 0, 255),  // Light Red
        cv::Scalar(255, 0, 128)   // Purple
    };
    
    return colorPalette[id % colorPalette.size()];
}

const std::vector<DetectedPerson>& PersonTracker::getTrackedPersons() const {
    return m_trackedPersons;
}

} // namespace hms
