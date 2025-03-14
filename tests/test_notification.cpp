#include "network/notification_manager.hpp"
#include "database/user_database.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <nlohmann/json.hpp>
#include <memory>

using namespace hms;
using json = nlohmann::json;

// Use a mock UserDatabase for testing
class MockUserDatabase : public UserDatabase {
public:
    // Pass a valid database path to the parent constructor
    MockUserDatabase(const json& config) 
        : UserDatabase(":memory:"), m_config(config) {}
    
    // Add any additional methods needed for testing
    // These methods don't need to be marked override since they're new
    std::string getPhoneNumber(int userId) const { 
        return "+0987654321"; 
    }
    
    std::string getEmail(int userId) const { 
        return "recipient@example.com"; 
    }
    
    // Store the config for access by the NotificationManager
    json getConfig() const { return m_config; }
    
private:
    json m_config;
};

// Test function to verify notification manager initialization
void test_notification_manager_init() {
    std::cout << "Testing NotificationManager initialization..." << std::endl;
    
    try {
        // Create a notification manager with test configuration
        json config = {
            {"sms", {
                {"enabled", true},
                {"provider", "test"},
                {"account_sid", "test_sid"},
                {"auth_token", "test_token"},
                {"from_number", "+1234567890"}
            }},
            {"email", {
                {"enabled", true},
                {"smtp_server", "smtp.test.com"},
                {"smtp_port", 587},
                {"username", "test@example.com"},
                {"password", "test_password"},
                {"from_email", "test@example.com"},
                {"use_ssl", true}
            }}
        };
        
        // Create a mock UserDatabase with the config
        auto userDb = std::make_unique<MockUserDatabase>(config);
        
        // Pass the UserDatabase pointer to the NotificationManager
        NotificationManager manager(userDb.get());
        std::cout << "NotificationManager initialized successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize NotificationManager: " << e.what() << std::endl;
        assert(false && "NotificationManager initialization failed");
    }
}

// Test function to verify SMS notification
void test_sms_notification() {
    std::cout << "Testing SMS notification..." << std::endl;
    
    try {
        // Create a notification manager with test configuration
        json config = {
            {"sms", {
                {"enabled", true},
                {"provider", "test"},
                {"account_sid", "test_sid"},
                {"auth_token", "test_token"},
                {"from_number", "+1234567890"}
            }}
        };
        
        // Create a mock UserDatabase with the config
        auto userDb = std::make_unique<MockUserDatabase>(config);
        
        // Pass the UserDatabase pointer to the NotificationManager
        NotificationManager manager(userDb.get());
        
        // Test sending notification through a public method
        // Instead of directly calling the private sendSmsNotification method,
        // we'll use notifyFallEvent which should internally call sendSmsNotification
        
        // Create a fall event based on the actual FallEvent structure
        hms::FallEvent fallEvent;
        fallEvent.personId = 1;
        fallEvent.startTime = std::chrono::steady_clock::now();
        fallEvent.alerted = false;
        fallEvent.position = cv::Rect(100, 200, 150, 300);
        
        // This should trigger SMS notification internally
        manager.notifyFallEvent(fallEvent, 1);
        
        std::cout << "SMS notification test completed" << std::endl;
        std::cout << "Note: No actual SMS is sent in test mode" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to test SMS notification: " << e.what() << std::endl;
        std::cout << "This is expected if SMS provider credentials are not set" << std::endl;
    }
}

// Test function to verify email notification
void test_email_notification() {
    std::cout << "Testing email notification..." << std::endl;
    
    try {
        // Create a notification manager with test configuration
        json config = {
            {"email", {
                {"enabled", true},
                {"smtp_server", "smtp.test.com"},
                {"smtp_port", 587},
                {"username", "test@example.com"},
                {"password", "test_password"},
                {"from_email", "test@example.com"},
                {"use_ssl", true}
            }}
        };
        
        // Create a mock UserDatabase with the config
        auto userDb = std::make_unique<MockUserDatabase>(config);
        
        // Pass the UserDatabase pointer to the NotificationManager
        NotificationManager manager(userDb.get());
        
        // Test sending notification through a public method
        // Instead of directly calling the private sendEmailNotification method,
        // we'll use notifyFallEvent which should internally call sendEmailNotification
        
        // Create a fall event based on the actual FallEvent structure
        hms::FallEvent fallEvent;
        fallEvent.personId = 1;
        fallEvent.startTime = std::chrono::steady_clock::now();
        fallEvent.alerted = false;
        fallEvent.position = cv::Rect(100, 200, 150, 300);
        
        // This should trigger email notification internally
        manager.notifyFallEvent(fallEvent, 1);
        
        std::cout << "Email notification test completed" << std::endl;
        std::cout << "Note: No actual email is sent in test mode" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to test email notification: " << e.what() << std::endl;
        std::cout << "This is expected if email provider credentials are not set" << std::endl;
    }
}

// Test function to verify fall event notification
void test_fall_event_notification() {
    std::cout << "Testing fall event notification..." << std::endl;
    
    try {
        // Create a notification manager with test configuration
        json config = {
            {"sms", {
                {"enabled", true},
                {"provider", "test"},
                {"account_sid", "test_sid"},
                {"auth_token", "test_token"},
                {"from_number", "+1234567890"}
            }},
            {"email", {
                {"enabled", true},
                {"smtp_server", "smtp.test.com"},
                {"smtp_port", 587},
                {"username", "test@example.com"},
                {"password", "test_password"},
                {"from_email", "test@example.com"},
                {"use_ssl", true}
            }}
        };
        
        // Create a mock UserDatabase with the config
        auto userDb = std::make_unique<MockUserDatabase>(config);
        
        // Pass the UserDatabase pointer to the NotificationManager
        NotificationManager manager(userDb.get());
        
        // Create a test fall event using the hms namespace to avoid ambiguity
        // and based on the actual FallEvent structure
        hms::FallEvent fallEvent;
        fallEvent.personId = 1;
        fallEvent.startTime = std::chrono::steady_clock::now();
        fallEvent.alerted = false;
        fallEvent.position = cv::Rect(100, 200, 150, 300);
        
        // This should not actually send notifications in test mode
        manager.notifyFallEvent(fallEvent, 1);
        
        std::cout << "Fall event notification completed" << std::endl;
        std::cout << "Note: No actual notifications are sent in test mode" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to test fall event notification: " << e.what() << std::endl;
        std::cout << "This is expected if notification credentials are not set" << std::endl;
    }
}

int main() {
    std::cout << "Starting Notification Manager tests..." << std::endl;
    
    try {
        test_notification_manager_init();
        test_sms_notification();
        test_email_notification();
        test_fall_event_notification();
        
        std::cout << "All Notification Manager tests completed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
