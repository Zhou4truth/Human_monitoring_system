// include/network/notification_manager.hpp
#pragma once

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include "database/user_database.hpp"
#include "detection/fall_detector.hpp"

namespace hms {

enum class NotificationStatus {
    PENDING,
    SENT,
    DELIVERED,
    READ,
    RESPONDED,
    FAILED
};

struct NotificationMessage {
    int userId;
    int personId;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    NotificationStatus status;
    std::string responseMessage;
    std::chrono::system_clock::time_point responseTimestamp;
};

class NotificationManager {
public:
    NotificationManager(UserDatabase* userDb);
    ~NotificationManager();
    
    void initialize();
    void shutdown();
    
    // Add a fall event to be notified
    void notifyFallEvent(const FallEvent& fallEvent, int userId);
    
    // Check for responses
    bool hasResponse(int userId, int personId);
    NotificationMessage getLatestResponse(int userId, int personId);
    
    // Register callback for when a response is received
    using ResponseCallback = std::function<void(const NotificationMessage&)>;
    void registerResponseCallback(ResponseCallback callback);
    
private:
    UserDatabase* m_userDb;
    std::atomic<bool> m_running;
    std::thread m_notificationThread;
    std::thread m_responseCheckThread;
    
    std::queue<NotificationMessage> m_notificationQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;
    
    std::map<std::pair<int, int>, NotificationMessage> m_activeNotifications;
    std::mutex m_activeNotificationsMutex;
    
    std::vector<ResponseCallback> m_responseCallbacks;
    std::mutex m_callbackMutex;
    
    // SMS/Email configuration
    std::string m_smsApiKey;
    std::string m_emailSmtpServer;
    std::string m_emailUsername;
    std::string m_emailPassword;
    
    void notificationThreadFunc();
    void responseCheckThreadFunc();
    
    bool sendSmsNotification(const std::string& phoneNumber, const std::string& message);
    bool sendEmailNotification(const std::string& email, const std::string& subject, const std::string& message);
    
    // Simulate response for testing (in real implementation, this would be an API endpoint)
    bool checkForResponses();
    void processResponse(const NotificationMessage& response);
};

} // namespace hms
