#include "network/notification_manager.hpp"
#include <iostream>
#include <sstream>
#include <curl/curl.h>

namespace hms {

// Callback function for CURL to handle response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch(std::bad_alloc& e) {
        return 0;
    }
}

NotificationManager::NotificationManager(UserDatabase* userDb)
    : m_userDb(userDb), m_running(false), 
      m_smsApiKey("YOUR_SMS_API_KEY"), // Replace with actual API key in production
      m_emailSmtpServer("smtp.example.com"),
      m_emailUsername("notifications@example.com"),
      m_emailPassword("your_password") { // Use secure storage in production
}

NotificationManager::~NotificationManager() {
    shutdown();
}

void NotificationManager::initialize() {
    if (m_running) {
        return;
    }
    
    m_running = true;
    
    // Initialize CURL globally
    curl_global_init(CURL_GLOBAL_ALL);
    
    // Start notification thread
    m_notificationThread = std::thread(&NotificationManager::notificationThreadFunc, this);
    
    // Start response check thread
    m_responseCheckThread = std::thread(&NotificationManager::responseCheckThreadFunc, this);
}

void NotificationManager::shutdown() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    // Notify threads to exit
    m_queueCV.notify_all();
    
    // Wait for threads to finish
    if (m_notificationThread.joinable()) {
        m_notificationThread.join();
    }
    
    if (m_responseCheckThread.joinable()) {
        m_responseCheckThread.join();
    }
    
    // Cleanup CURL
    curl_global_cleanup();
}

void NotificationManager::notifyFallEvent(const FallEvent& fallEvent, int userId) {
    User user = m_userDb->getUserById(userId);
    if (user.id == -1) {
        std::cerr << "User not found: " << userId << std::endl;
        return;
    }
    
    // Create notification message
    std::stringstream ss;
    ss << "EMERGENCY ALERT: " << user.name << " has fallen and may need assistance. "
       << "This alert was triggered at " 
       << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
       << ". Please respond to this message to confirm you are taking action.";
    
    std::string message = ss.str();
    
    // Queue notifications for all emergency contacts
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        for (const auto& contact : user.emergencyContacts) {
            NotificationMessage notification;
            notification.userId = userId;
            notification.personId = fallEvent.personId;
            notification.message = message;
            notification.timestamp = std::chrono::system_clock::now();
            notification.status = NotificationStatus::PENDING;
            
            m_notificationQueue.push(notification);
            
            // Store in active notifications
            std::lock_guard<std::mutex> activeLock(m_activeNotificationsMutex);
            m_activeNotifications[std::make_pair(userId, fallEvent.personId)] = notification;
        }
        
        // Also notify family doctor if available
        if (!user.familyDoctor.name.empty()) {
            NotificationMessage notification;
            notification.userId = userId;
            notification.personId = fallEvent.personId;
            notification.message = message + " (Medical assistance may be required)";
            notification.timestamp = std::chrono::system_clock::now();
            notification.status = NotificationStatus::PENDING;
            
            m_notificationQueue.push(notification);
        }
    }
    
    // Notify the thread that new notifications are available
    m_queueCV.notify_one();
}

bool NotificationManager::hasResponse(int userId, int personId) {
    std::lock_guard<std::mutex> lock(m_activeNotificationsMutex);
    
    auto it = m_activeNotifications.find(std::make_pair(userId, personId));
    if (it != m_activeNotifications.end()) {
        return it->second.status == NotificationStatus::RESPONDED;
    }
    
    return false;
}

NotificationMessage NotificationManager::getLatestResponse(int userId, int personId) {
    std::lock_guard<std::mutex> lock(m_activeNotificationsMutex);
    
    auto it = m_activeNotifications.find(std::make_pair(userId, personId));
    if (it != m_activeNotifications.end()) {
        return it->second;
    }
    
    // Return empty notification if not found
    NotificationMessage empty;
    empty.userId = -1;
    empty.personId = -1;
    return empty;
}

void NotificationManager::registerResponseCallback(ResponseCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_responseCallbacks.push_back(callback);
}

void NotificationManager::notificationThreadFunc() {
    while (m_running) {
        NotificationMessage notification;
        
        // Wait for a notification to be available
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCV.wait(lock, [this] { 
                return !m_running || !m_notificationQueue.empty(); 
            });
            
            if (!m_running) {
                break;
            }
            
            if (m_notificationQueue.empty()) {
                continue;
            }
            
            notification = m_notificationQueue.front();
            m_notificationQueue.pop();
        }
        
        // Get user information
        User user = m_userDb->getUserById(notification.userId);
        if (user.id == -1) {
            std::cerr << "User not found: " << notification.userId << std::endl;
            continue;
        }
        
        bool notificationSent = false;
        
        // Send notifications to all emergency contacts
        for (const auto& contact : user.emergencyContacts) {
            // Try SMS first
            if (!contact.phone.empty()) {
                if (sendSmsNotification(contact.phone, notification.message)) {
                    notificationSent = true;
                }
            }
            
            // Try email as backup
            if (!contact.email.empty()) {
                std::string subject = "EMERGENCY ALERT: Fall Detected";
                if (sendEmailNotification(contact.email, subject, notification.message)) {
                    notificationSent = true;
                }
            }
        }
        
        // Also notify family doctor if available
        if (!user.familyDoctor.name.empty()) {
            if (!user.familyDoctor.phone.empty()) {
                sendSmsNotification(user.familyDoctor.phone, notification.message);
            }
            
            if (!user.familyDoctor.email.empty()) {
                std::string subject = "MEDICAL EMERGENCY ALERT: Fall Detected";
                sendEmailNotification(user.familyDoctor.email, subject, notification.message);
            }
        }
        
        // Update notification status
        {
            std::lock_guard<std::mutex> lock(m_activeNotificationsMutex);
            auto it = m_activeNotifications.find(std::make_pair(notification.userId, notification.personId));
            if (it != m_activeNotifications.end()) {
                it->second.status = notificationSent ? NotificationStatus::SENT : NotificationStatus::FAILED;
            }
        }
    }
}

void NotificationManager::responseCheckThreadFunc() {
    while (m_running) {
        // Check for responses every 5 seconds
        if (checkForResponses()) {
            // Process any new responses
        }
        
        // Sleep for 5 seconds
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool NotificationManager::sendSmsNotification(const std::string& phoneNumber, const std::string& message) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL for SMS" << std::endl;
        return false;
    }
    
    // This is a placeholder for a real SMS API integration
    // In a real implementation, you would integrate with Twilio, Nexmo, or another SMS service
    
    std::string url = "https://api.example.com/sms";
    std::string postFields = "apikey=" + m_smsApiKey + 
                            "&to=" + phoneNumber + 
                            "&message=" + curl_easy_escape(curl, message.c_str(), 0);
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
    
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "SMS send failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    // In a real implementation, parse the response to confirm delivery
    std::cout << "SMS notification sent to " << phoneNumber << std::endl;
    return true;
}

bool NotificationManager::sendEmailNotification(const std::string& email, const std::string& subject, const std::string& message) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL for email" << std::endl;
        return false;
    }
    
    // This is a placeholder for a real email API integration
    // In a real implementation, you would use libcurl to connect to an SMTP server
    
    struct curl_slist* recipients = NULL;
    recipients = curl_slist_append(recipients, email.c_str());
    
    std::string emailContent = "To: " + email + "\r\n"
                             + "From: " + m_emailUsername + "\r\n"
                             + "Subject: " + subject + "\r\n"
                             + "\r\n"
                             + message;
    
    curl_easy_setopt(curl, CURLOPT_URL, m_emailSmtpServer.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, m_emailUsername.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READDATA, emailContent.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME, m_emailUsername.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, m_emailPassword.c_str());
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "Email send failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    std::cout << "Email notification sent to " << email << std::endl;
    return true;
}

bool NotificationManager::checkForResponses() {
    // In a real implementation, this would check an API endpoint or SMS/Email inbox
    // For this example, we'll simulate responses randomly
    
    std::lock_guard<std::mutex> lock(m_activeNotificationsMutex);
    
    bool newResponseFound = false;
    
    for (auto& pair : m_activeNotifications) {
        auto& notification = pair.second;
        
        // Only check notifications that have been sent but not responded to
        if (notification.status == NotificationStatus::SENT || 
            notification.status == NotificationStatus::DELIVERED || 
            notification.status == NotificationStatus::READ) {
            
            // Simulate a 10% chance of receiving a response
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_real_distribution<> dis(0.0, 1.0);
            
            if (dis(gen) < 0.1) {  // 10% chance
                notification.status = NotificationStatus::RESPONDED;
                notification.responseMessage = "I'm on my way to help. ETA 10 minutes.";
                notification.responseTimestamp = std::chrono::system_clock::now();
                
                newResponseFound = true;
                
                // Process the response
                processResponse(notification);
            }
        }
    }
    
    return newResponseFound;
}

void NotificationManager::processResponse(const NotificationMessage& response) {
    // Call all registered callbacks
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_responseCallbacks) {
        callback(response);
    }
    
    // Log the response
    std::cout << "Response received for user " << response.userId 
              << ", person " << response.personId << ": " 
              << response.responseMessage << std::endl;
}

} // namespace hms
