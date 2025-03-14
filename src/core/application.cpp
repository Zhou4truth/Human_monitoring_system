#include "core/application.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace hms {

Application::Application()
    : m_running(false),
      m_fallDetectionEnabled(true),
      m_privacyProtectionEnabled(true),
      m_recordingEnabled(true),
      m_recordingDirectory("recordings"),
      m_activeCameraIndex(0) {
}

Application::~Application() {
    stop();
}

bool Application::initialize(const std::string& configPath) {
    try {
        // Create directories if they don't exist
        if (!fs::exists(m_recordingDirectory)) {
            fs::create_directories(m_recordingDirectory);
        }
        
        // Initialize database
        m_userDatabase = std::make_unique<UserDatabase>("hms_database.db");
        if (!m_userDatabase->initialize()) {
            std::cerr << "Failed to initialize user database" << std::endl;
            return false;
        }
        
        // Initialize camera manager
        m_cameraManager = std::make_unique<CameraManager>();
        
        // Initialize human detector with YOLO model
        m_humanDetector = std::make_unique<HumanDetector>("models/yolov8n.onnx", 0.5f, 0.45f);
        if (!m_humanDetector->initialize()) {
            std::cerr << "Failed to initialize human detector" << std::endl;
            return false;
        }
        
        // Initialize person tracker
        m_personTracker = std::make_unique<PersonTracker>();
        
        // Initialize fall detector
        m_fallDetector = std::make_unique<FallDetector>(10); // 10 seconds threshold
        
        // Initialize privacy protector
        m_privacyProtector = std::make_unique<PrivacyProtector>("models/privacy_model.onnx");
        if (!m_privacyProtector->initialize()) {
            std::cerr << "Failed to initialize privacy protector" << std::endl;
            return false;
        }
        
        // Initialize notification manager
        m_notificationManager = std::make_unique<NotificationManager>(m_userDatabase.get());
        m_notificationManager->initialize();
        
        // Load configuration if file exists
        if (fs::exists(configPath)) {
            std::ifstream configFile(configPath);
            if (configFile.is_open()) {
                try {
                    json config;
                    configFile >> config;
                    
                    // Load cameras
                    if (config.contains("cameras") && config["cameras"].is_array()) {
                        for (const auto& camera : config["cameras"]) {
                            std::string uri = camera["uri"];
                            std::string typeStr = camera["type"];
                            
                            Camera::ConnectionType type;
                            if (typeStr == "USB") {
                                type = Camera::ConnectionType::USB;
                            } else if (typeStr == "RTSP") {
                                type = Camera::ConnectionType::RTSP;
                            } else if (typeStr == "HTTP") {
                                type = Camera::ConnectionType::HTTP;
                            } else if (typeStr == "MJPEG") {
                                type = Camera::ConnectionType::MJPEG;
                            } else {
                                continue;
                            }
                            
                            addCamera(uri, type);
                        }
                    }
                    
                    // Load settings
                    if (config.contains("settings")) {
                        if (config["settings"].contains("fallDetectionEnabled")) {
                            m_fallDetectionEnabled = config["settings"]["fallDetectionEnabled"];
                        }
                        
                        if (config["settings"].contains("privacyProtectionEnabled")) {
                            m_privacyProtectionEnabled = config["settings"]["privacyProtectionEnabled"];
                        }
                        
                        if (config["settings"].contains("recordingEnabled")) {
                            m_recordingEnabled = config["settings"]["recordingEnabled"];
                        }
                        
                        if (config["settings"].contains("recordingDirectory")) {
                            m_recordingDirectory = config["settings"]["recordingDirectory"];
                            
                            if (!fs::exists(m_recordingDirectory)) {
                                fs::create_directories(m_recordingDirectory);
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing config file: " << e.what() << std::endl;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing application: " << e.what() << std::endl;
        return false;
    }
}

void Application::run() {
    if (m_running) {
        return;
    }
    
    m_running = true;
    
    // Initialize frame buffers
    size_t numCameras = m_cameraManager->getCameraCount();
    {
        std::lock_guard<std::mutex> lock(m_framesMutex);
        m_cameraFrames.resize(numCameras);
    }
    
    // Initialize video writers if recording is enabled
    if (m_recordingEnabled) {
        m_videoWriters.resize(numCameras);
        m_recordingStartTime = std::chrono::system_clock::now();
        
        for (size_t i = 0; i < numCameras; i++) {
            auto timePoint = std::chrono::system_clock::now();
            auto timeT = std::chrono::system_clock::to_time_t(timePoint);
            std::tm* now = std::localtime(&timeT);
            
            char buffer[128];
            strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", now);
            
            std::string filename = m_recordingDirectory + "/camera_" + 
                                  std::to_string(i) + "_" + buffer + ".mp4";
            
            m_videoWriters[i].open(filename, 
                                  cv::VideoWriter::fourcc('a', 'v', 'c', '1'), 
                                  30, cv::Size(1280, 720));
        }
    }
    
    // Start processing thread
    m_processingThread = std::thread(&Application::processingThreadFunc, this);
    
    // Start UI thread
    m_uiThread = std::thread(&Application::uiThreadFunc, this);
}

void Application::stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    // Wait for threads to finish
    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }
    
    if (m_uiThread.joinable()) {
        m_uiThread.join();
    }
    
    // Close video writers
    for (auto& writer : m_videoWriters) {
        if (writer.isOpened()) {
            writer.release();
        }
    }
    m_videoWriters.clear();
    
    // Shutdown notification manager
    if (m_notificationManager) {
        m_notificationManager->shutdown();
    }
}

bool Application::addCamera(const std::string& uri, Camera::ConnectionType type) {
    if (m_cameraManager->getCameraCount() >= 4) {
        std::cerr << "Maximum number of cameras (4) already added" << std::endl;
        return false;
    }
    
    bool result = m_cameraManager->addCamera(uri, type);
    
    if (result) {
        // Resize frame buffers
        std::lock_guard<std::mutex> lock(m_framesMutex);
        m_cameraFrames.resize(m_cameraManager->getCameraCount());
        
        // Add video writer if recording is enabled
        if (m_recordingEnabled) {
            size_t index = m_cameraManager->getCameraCount() - 1;
            
            auto timePoint = std::chrono::system_clock::now();
            auto timeT = std::chrono::system_clock::to_time_t(timePoint);
            std::tm* now = std::localtime(&timeT);
            
            char buffer[128];
            strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", now);
            
            std::string filename = m_recordingDirectory + "/camera_" + 
                                  std::to_string(index) + "_" + buffer + ".mp4";
            
            m_videoWriters.resize(m_cameraManager->getCameraCount());
            m_videoWriters[index].open(filename, 
                                      cv::VideoWriter::fourcc('a', 'v', 'c', '1'), 
                                      30, cv::Size(1280, 720));
        }
    }
    
    return result;
}

bool Application::removeCamera(const std::string& id) {
    size_t cameraCount = m_cameraManager->getCameraCount();
    bool result = m_cameraManager->removeCamera(id);
    
    if (result) {
        // Resize frame buffers
        std::lock_guard<std::mutex> lock(m_framesMutex);
        m_cameraFrames.resize(m_cameraManager->getCameraCount());
        
        // Close and remove video writer if recording is enabled
        if (m_recordingEnabled && m_cameraManager->getCameraCount() < cameraCount) {
            for (size_t i = 0; i < m_videoWriters.size(); i++) {
                if (m_videoWriters[i].isOpened()) {
                    m_videoWriters[i].release();
                }
            }
            
            m_videoWriters.resize(m_cameraManager->getCameraCount());
            
            // Recreate video writers
            for (size_t i = 0; i < m_cameraManager->getCameraCount(); i++) {
                auto timePoint = std::chrono::system_clock::now();
                auto timeT = std::chrono::system_clock::to_time_t(timePoint);
                std::tm* now = std::localtime(&timeT);
                
                char buffer[128];
                strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", now);
                
                std::string filename = m_recordingDirectory + "/camera_" + 
                                      std::to_string(i) + "_" + buffer + ".mp4";
                
                m_videoWriters[i].open(filename, 
                                      cv::VideoWriter::fourcc('a', 'v', 'c', '1'), 
                                      30, cv::Size(1280, 720));
            }
        }
        
        // Update active camera index if needed
        std::lock_guard<std::mutex> activeLock(m_activeCameraIndexMutex);
        if (m_activeCameraIndex >= m_cameraManager->getCameraCount() && m_cameraManager->getCameraCount() > 0) {
            m_activeCameraIndex = m_cameraManager->getCameraCount() - 1;
        }
    }
    
    return result;
}

size_t Application::getCameraCount() const {
    return m_cameraManager->getCameraCount();
}

bool Application::addUser(User& user) {
    return m_userDatabase->addUser(user);
}

bool Application::updateUser(const User& user) {
    return m_userDatabase->updateUser(user);
}

bool Application::deleteUser(int userId) {
    return m_userDatabase->deleteUser(userId);
}

User Application::getUserById(int userId) {
    return m_userDatabase->getUserById(userId);
}

std::vector<User> Application::getAllUsers() {
    return m_userDatabase->getAllUsers();
}

void Application::setActiveCameraIndex(size_t index) {
    std::lock_guard<std::mutex> lock(m_activeCameraIndexMutex);
    if (index < m_cameraManager->getCameraCount()) {
        m_activeCameraIndex = index;
    }
}

size_t Application::getActiveCameraIndex() const {
    // Use const_cast to allow locking the mutex in a const method
    std::lock_guard<std::mutex> lock(*const_cast<std::mutex*>(&m_activeCameraIndexMutex));
    return m_activeCameraIndex;
}

void Application::enableFallDetection(bool enable) {
    m_fallDetectionEnabled = enable;
}

bool Application::isFallDetectionEnabled() const {
    return m_fallDetectionEnabled;
}

void Application::enablePrivacyProtection(bool enable) {
    m_privacyProtectionEnabled = enable;
}

bool Application::isPrivacyProtectionEnabled() const {
    return m_privacyProtectionEnabled;
}

void Application::enableRecording(bool enable) {
    if (m_recordingEnabled == enable) {
        return;
    }
    
    m_recordingEnabled = enable;
    
    if (enable) {
        // Start recording
        m_recordingStartTime = std::chrono::system_clock::now();
        
        size_t numCameras = m_cameraManager->getCameraCount();
        m_videoWriters.resize(numCameras);
        
        for (size_t i = 0; i < numCameras; i++) {
            auto timePoint = std::chrono::system_clock::now();
            auto timeT = std::chrono::system_clock::to_time_t(timePoint);
            std::tm* now = std::localtime(&timeT);
            
            char buffer[128];
            strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", now);
            
            std::string filename = m_recordingDirectory + "/camera_" + 
                                  std::to_string(i) + "_" + buffer + ".mp4";
            
            m_videoWriters[i].open(filename, 
                                  cv::VideoWriter::fourcc('a', 'v', 'c', '1'), 
                                  30, cv::Size(1280, 720));
        }
    } else {
        // Stop recording
        for (auto& writer : m_videoWriters) {
            if (writer.isOpened()) {
                writer.release();
            }
        }
        m_videoWriters.clear();
    }
}

bool Application::isRecordingEnabled() const {
    return m_recordingEnabled;
}

std::string Application::getRecordingDirectory() const {
    return m_recordingDirectory;
}

void Application::setRecordingDirectory(const std::string& directory) {
    if (m_recordingDirectory == directory) {
        return;
    }
    
    // Create directory if it doesn't exist
    if (!fs::exists(directory)) {
        fs::create_directories(directory);
    }
    
    m_recordingDirectory = directory;
    
    // If recording is enabled, restart recording with new directory
    if (m_recordingEnabled) {
        enableRecording(false);
        enableRecording(true);
    }
}

void Application::processingThreadFunc() {
    while (m_running) {
        size_t numCameras = m_cameraManager->getCameraCount();
        
        if (numCameras == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Process each camera
        for (size_t i = 0; i < numCameras; i++) {
            Camera* camera = m_cameraManager->getCamera(i);
            if (!camera || !camera->isConnected()) {
                continue;
            }
            
            // Get frame from camera
            cv::Mat frame = camera->getFrame();
            if (frame.empty()) {
                continue;
            }
            
            // Process frame
            processFrame(i, frame);
            
            // Store processed frame
            {
                std::lock_guard<std::mutex> lock(m_framesMutex);
                if (i < m_cameraFrames.size()) {
                    m_cameraFrames[i] = frame.clone();
                }
            }
            
            // Record frame if enabled
            if (m_recordingEnabled && i < m_videoWriters.size() && m_videoWriters[i].isOpened()) {
                m_videoWriters[i].write(frame);
            }
        }
        
        // Handle fall events
        handleFallEvents();
        
        // Clean up old recordings
        cleanupOldRecordings();
        
        // Clean up old movement records
        cleanupOldMovementRecords();
        
        // Sleep to limit CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

void Application::uiThreadFunc() {
    // Create main window
    cv::namedWindow("Human Monitoring System", cv::WINDOW_NORMAL);
    cv::resizeWindow("Human Monitoring System", 1280, 720);
    
    // Set mouse callback
    cv::setMouseCallback("Human Monitoring System", mouseCallback, this);
    
    while (m_running) {
        // Update UI
        updateUI();
        
        // Process UI events
        int key = cv::waitKey(30);
        
        // Handle key presses
        if (key == 27) {  // ESC key
            m_running = false;
            break;
        } else if (key >= '1' && key <= '4') {
            // Switch active camera
            size_t index = key - '1';
            if (index < m_cameraManager->getCameraCount()) {
                setActiveCameraIndex(index);
            }
        } else if (key == 'f' || key == 'F') {
            // Toggle fall detection
            enableFallDetection(!m_fallDetectionEnabled);
        } else if (key == 'p' || key == 'P') {
            // Toggle privacy protection
            enablePrivacyProtection(!m_privacyProtectionEnabled);
        } else if (key == 'r' || key == 'R') {
            // Toggle recording
            enableRecording(!m_recordingEnabled);
        }
    }
    
    cv::destroyAllWindows();
}

void Application::processFrame(size_t cameraIndex, cv::Mat& frame) {
    // Detect persons
    std::vector<DetectedPerson> persons = m_humanDetector->detectPersons(frame);
    
    // Track persons
    m_personTracker->update(persons, frame);
    
    // Apply privacy protection if enabled
    if (m_privacyProtectionEnabled) {
        frame = m_privacyProtector->applyPrivacyFilters(frame, persons);
    }
    
    // Analyze for falls if enabled
    if (m_fallDetectionEnabled) {
        m_fallDetector->analyze(persons, frame);
    }
    
    // Draw bounding boxes
    drawPersonBoundingBoxes(frame, persons);
    
    // Save movement records
    for (const auto& person : persons) {
        // For now, we'll use -1 as the userId since we don't have face recognition yet
        saveMovementRecord(-1, person.id, person.boundingBox);
    }
}

void Application::updateUI() {
    size_t numCameras = m_cameraManager->getCameraCount();
    
    if (numCameras == 0) {
        // Show "No cameras connected" message
        cv::Mat noCamera(720, 1280, CV_8UC3, cv::Scalar(0, 0, 0));
        cv::putText(noCamera, "No cameras connected", cv::Point(480, 360),
                   cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);
        cv::imshow("Human Monitoring System", noCamera);
        return;
    }
    
    // Get active camera index
    size_t activeCameraIndex;
    {
        std::lock_guard<std::mutex> lock(m_activeCameraIndexMutex);
        activeCameraIndex = m_activeCameraIndex;
    }
    
    if (activeCameraIndex >= numCameras) {
        activeCameraIndex = 0;
    }
    
    // Create UI layout
    cv::Mat ui(720, 1280, CV_8UC3, cv::Scalar(0, 0, 0));
    
    // Get frames
    std::vector<cv::Mat> frames;
    {
        std::lock_guard<std::mutex> lock(m_framesMutex);
        frames = m_cameraFrames;
    }
    
    // Draw active camera in main area
    if (activeCameraIndex < frames.size() && !frames[activeCameraIndex].empty()) {
        cv::Mat activeFrame = frames[activeCameraIndex].clone();
        cv::resize(activeFrame, activeFrame, cv::Size(960, 720));
        activeFrame.copyTo(ui(cv::Rect(0, 0, 960, 720)));
    }
    
    // Draw sidebar with all cameras
    for (size_t i = 0; i < numCameras && i < frames.size(); i++) {
        if (frames[i].empty()) {
            continue;
        }
        
        cv::Mat thumbnail;
        cv::resize(frames[i], thumbnail, cv::Size(320, 180));
        
        int y = i * 180;
        thumbnail.copyTo(ui(cv::Rect(960, y, 320, 180)));
        
        // Highlight active camera
        if (i == activeCameraIndex) {
            cv::rectangle(ui, cv::Rect(960, y, 320, 180), cv::Scalar(0, 255, 0), 2);
        }
        
        // Add camera label
        cv::putText(ui, "Camera " + std::to_string(i + 1), cv::Point(970, y + 20),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    }
    
    // Add status bar
    cv::rectangle(ui, cv::Rect(0, 720 - 30, 1280, 30), cv::Scalar(50, 50, 50), -1);
    
    // Show status information
    std::string statusText = "Fall Detection: ";
    statusText += m_fallDetectionEnabled ? "ON" : "OFF";
    statusText += " | Privacy Protection: ";
    statusText += m_privacyProtectionEnabled ? "ON" : "OFF";
    statusText += " | Recording: ";
    statusText += m_recordingEnabled ? "ON" : "OFF";
    
    cv::putText(ui, statusText, cv::Point(10, 720 - 10),
               cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    
    // Show UI
    cv::imshow("Human Monitoring System", ui);
}

void Application::handleFallEvents() {
    if (!m_fallDetectionEnabled) {
        return;
    }
    
    // Get active fall events
    std::vector<FallEvent> fallEvents = m_fallDetector->getActiveFallEvents();
    
    // Get new alerts
    std::vector<int> newAlerts = m_fallDetector->getNewAlerts();
    
    // Process new alerts
    for (int personId : newAlerts) {
        // Find the fall event
        auto it = std::find_if(fallEvents.begin(), fallEvents.end(),
                              [personId](const FallEvent& event) {
                                  return event.personId == personId;
                              });
        
        if (it != fallEvents.end()) {
            // TODO: In a real implementation, we would use face recognition to identify the person
            // For now, we'll notify all users
            std::vector<User> users = m_userDatabase->getAllUsers();
            
            for (const auto& user : users) {
                m_notificationManager->notifyFallEvent(*it, user.id);
            }
        }
    }
}

void Application::cleanupOldRecordings() {
    if (!m_recordingEnabled) {
        return;
    }
    
    // Check if we need to create new recordings (every 24 hours)
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::hours>(now - m_recordingStartTime).count();
    
    if (duration >= 24) {
        // Close current video writers
        for (auto& writer : m_videoWriters) {
            if (writer.isOpened()) {
                writer.release();
            }
        }
        
        // Create new video writers
        m_recordingStartTime = now;
        size_t numCameras = m_cameraManager->getCameraCount();
        
        for (size_t i = 0; i < numCameras; i++) {
            auto timeT = std::chrono::system_clock::to_time_t(now);
            std::tm* timeInfo = std::localtime(&timeT);
            
            char buffer[128];
            strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", timeInfo);
            
            std::string filename = m_recordingDirectory + "/camera_" + 
                                  std::to_string(i) + "_" + buffer + ".mp4";
            
            m_videoWriters[i].open(filename, 
                                  cv::VideoWriter::fourcc('a', 'v', 'c', '1'), 
                                  30, cv::Size(1280, 720));
        }
        
        // Delete old recordings (older than 24 hours)
        try {
            for (const auto& entry : fs::directory_iterator(m_recordingDirectory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".mp4") {
                    auto fileTime = fs::last_write_time(entry.path());
                    // Convert to time_t for comparison (C++17 compatible)
                    auto fileTimeT = fs::last_write_time(entry.path()).time_since_epoch().count();
                    auto nowTimeT = std::chrono::system_clock::now().time_since_epoch().count();
                    auto fileAge = (nowTimeT - fileTimeT) / 3600000000000; // Approximate hours
                    
                    if (fileAge > 24) {
                        fs::remove(entry.path());
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error cleaning up old recordings: " << e.what() << std::endl;
        }
    }
}

void Application::saveMovementRecord(int userId, int personId, const cv::Rect& position) {
    MovementRecord record;
    record.userId = userId;
    record.personId = personId;
    record.timestamp = std::chrono::system_clock::now();
    record.position = position;
    
    std::lock_guard<std::mutex> lock(m_historyMutex);
    m_movementHistory.push_back(record);
}

void Application::cleanupOldMovementRecords() {
    auto now = std::chrono::system_clock::now();
    
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    // Remove records older than 24 hours
    m_movementHistory.erase(
        std::remove_if(m_movementHistory.begin(), m_movementHistory.end(),
                      [&now](const MovementRecord& record) {
                          auto age = std::chrono::duration_cast<std::chrono::hours>(
                              now - record.timestamp).count();
                          return age >= 24;
                      }),
        m_movementHistory.end());
}

void Application::drawPersonBoundingBoxes(cv::Mat& frame, const std::vector<DetectedPerson>& persons) {
    for (const auto& person : persons) {
        // Draw bounding box
        cv::rectangle(frame, person.boundingBox, person.color, 2);
        
        // Draw person ID
        std::string idText = "Person " + std::to_string(person.id);
        cv::putText(frame, idText, 
                   cv::Point(person.boundingBox.x, person.boundingBox.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, person.color, 2);
        
        // Draw fall status if fallen
        if (person.isFallen) {
            cv::putText(frame, "FALLEN", 
                       cv::Point(person.boundingBox.x, person.boundingBox.y + person.boundingBox.height + 20),
                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);
        }
        
        // Draw user info if available
        if (!person.name.empty()) {
            drawUserInfo(frame, person);
        }
    }
}

void Application::drawUserInfo(cv::Mat& frame, const DetectedPerson& person) {
    int x = person.boundingBox.x;
    int y = person.boundingBox.y + person.boundingBox.height + 25;
    
    // Draw name
    cv::putText(frame, person.name, cv::Point(x, y),
               cv::FONT_HERSHEY_SIMPLEX, 0.6, person.color, 2);
}

void Application::handleMouseClick(int event, int x, int y) {
    if (event != cv::EVENT_LBUTTONDOWN) {
        return;
    }
    
    size_t numCameras = m_cameraManager->getCameraCount();
    
    // Check if click is in sidebar
    if (x >= 960 && x < 1280) {
        // Calculate which camera was clicked
        int cameraIndex = y / 180;
        
        if (cameraIndex >= 0 && cameraIndex < static_cast<int>(numCameras)) {
            setActiveCameraIndex(cameraIndex);
        }
    }
}

void Application::mouseCallback(int event, int x, int y, int flags, void* userdata) {
    Application* app = static_cast<Application*>(userdata);
    if (app) {
        app->handleMouseClick(event, x, y);
    }
}

bool Application::isRunning() const {
    return m_running;
}

bool Application::addCamera(const std::string& uri, Camera::ConnectionType type, const std::string& name) {
    // Call the original addCamera method and then set the name if successful
    if (addCamera(uri, type)) {
        // Find the camera and set its name
        // This is a simplified implementation
        return true;
    }
    return false;
}

bool Application::removeCamera(int index) {
    // Convert index to camera ID and call the original removeCamera method
    // This is a simplified implementation
    if (index >= 0 && index < static_cast<int>(getCameraCount())) {
        // Get camera ID at index and remove it
        std::string cameraId = std::to_string(index); // Placeholder, should get actual ID
        return removeCamera(cameraId);
    }
    return false;
}

Application::CameraInfo Application::getCameraInfo(size_t index) const {
    // Return camera information for the given index
    // This is a simplified implementation
    CameraInfo info;
    info.id = std::to_string(index);
    info.name = "Camera " + std::to_string(index);
    info.uri = "camera://" + std::to_string(index);
    info.type = Camera::ConnectionType::USB;
    info.isConnected = true;
    return info;
}

cv::Mat Application::getProcessedFrame(size_t cameraIndex) {
    // Return the processed frame for the given camera index
    // This is a simplified implementation
    cv::Mat frame;
    std::lock_guard<std::mutex> lock(m_framesMutex);
    if (cameraIndex < m_cameraFrames.size()) {
        frame = m_cameraFrames[cameraIndex].clone();
    } else {
        // Return an empty frame if the camera index is invalid
        frame = cv::Mat(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
        cv::putText(frame, "No camera feed available", cv::Point(50, 240), 
                   cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
    }
    return frame;
}

UserDatabase& Application::getUserDatabase() {
    return *m_userDatabase;
}

} // namespace hms
