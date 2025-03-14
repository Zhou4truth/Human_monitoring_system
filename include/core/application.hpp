// include/core/application.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <opencv2/opencv.hpp>

#include "core/camera.hpp"
#include "database/user_database.hpp"
#include "detection/human_detector.hpp"
#include "detection/fall_detector.hpp"
#include "detection/privacy_protector.hpp"
#include "network/notification_manager.hpp"

namespace hms {

class Application {
public:
    Application();
    ~Application();
    
    bool initialize(const std::string& configPath = "config.json");
    void run();
    void stop();
    
    // Camera management
    bool addCamera(const std::string& uri, Camera::ConnectionType type);
    bool addCamera(const std::string& uri, Camera::ConnectionType type, const std::string& name);
    bool removeCamera(const std::string& id);
    bool removeCamera(int index);
    size_t getCameraCount() const;
    
    // Camera information
    struct CameraInfo {
        std::string id;
        std::string name;
        std::string uri;
        Camera::ConnectionType type;
        bool isConnected;
    };
    
    CameraInfo getCameraInfo(size_t index) const;
    cv::Mat getProcessedFrame(size_t cameraIndex);
    
    // User database management
    bool addUser(User& user);
    bool updateUser(const User& user);
    bool deleteUser(int userId);
    User getUserById(int userId);
    std::vector<User> getAllUsers();
    UserDatabase& getUserDatabase();
    
    // UI interaction
    void setActiveCameraIndex(size_t index);
    size_t getActiveCameraIndex() const;
    bool isRunning() const;
    
    // Fall detection and notification
    void enableFallDetection(bool enable);
    bool isFallDetectionEnabled() const;
    
    // Privacy protection
    void enablePrivacyProtection(bool enable);
    bool isPrivacyProtectionEnabled() const;
    
    // Recording management
    void enableRecording(bool enable);
    bool isRecordingEnabled() const;
    std::string getRecordingDirectory() const;
    void setRecordingDirectory(const std::string& directory);
    
private:
    // Core components
    std::unique_ptr<CameraManager> m_cameraManager;
    std::unique_ptr<UserDatabase> m_userDatabase;
    std::unique_ptr<HumanDetector> m_humanDetector;
    std::unique_ptr<FallDetector> m_fallDetector;
    std::unique_ptr<PrivacyProtector> m_privacyProtector;
    std::unique_ptr<NotificationManager> m_notificationManager;
    std::unique_ptr<PersonTracker> m_personTracker;
    
    // Application state
    std::atomic<bool> m_running;
    std::atomic<bool> m_fallDetectionEnabled;
    std::atomic<bool> m_privacyProtectionEnabled;
    std::atomic<bool> m_recordingEnabled;
    std::string m_recordingDirectory;
    
    // Active camera
    size_t m_activeCameraIndex;
    std::mutex m_activeCameraIndexMutex;
    
    // Processing threads
    std::thread m_processingThread;
    std::thread m_uiThread;
    
    // Frame buffers
    std::vector<cv::Mat> m_cameraFrames;
    std::mutex m_framesMutex;
    
    // Recording
    std::vector<cv::VideoWriter> m_videoWriters;
    std::chrono::system_clock::time_point m_recordingStartTime;
    
    // Historical data (last 24 hours)
    struct MovementRecord {
        int userId;
        int personId;
        std::chrono::system_clock::time_point timestamp;
        cv::Rect position;
    };
    std::vector<MovementRecord> m_movementHistory;
    std::mutex m_historyMutex;
    
    // Methods
    void processingThreadFunc();
    void uiThreadFunc();
    void processFrame(size_t cameraIndex, cv::Mat& frame);
    void updateUI();
    void handleFallEvents();
    void cleanupOldRecordings();
    void saveMovementRecord(int userId, int personId, const cv::Rect& position);
    void cleanupOldMovementRecords();
    
    // UI helper methods
    void drawPersonBoundingBoxes(cv::Mat& frame, const std::vector<DetectedPerson>& persons);
    void drawUserInfo(cv::Mat& frame, const DetectedPerson& person);
    void handleMouseClick(int event, int x, int y);
    static void mouseCallback(int event, int x, int y, int flags, void* userdata);
};

} // namespace hms
