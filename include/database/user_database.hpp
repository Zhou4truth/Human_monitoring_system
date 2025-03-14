// include/database/user_database.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sqlite3.h>

namespace hms {

struct EmergencyContact {
    std::string name;
    std::string phone;
    std::string email;
    std::string address;
    std::string relationship;
};

struct Doctor {
    std::string name;
    std::string phone;
    std::string email;
    std::string address;
    std::string specialization;
};

struct User {
    int id;
    std::string name;
    std::vector<EmergencyContact> emergencyContacts;
    Doctor familyDoctor;
    std::string notes;
    std::string imageReference;  // Path to user's reference image for facial recognition
};

class UserDatabase {
public:
    UserDatabase(const std::string& dbPath);
    ~UserDatabase();
    
    bool initialize();
    bool isInitialized() const;
    
    // User management
    bool addUser(User& user);  // Updates user.id if successful
    bool updateUser(const User& user);
    bool deleteUser(int userId);
    User getUserById(int userId);
    std::vector<User> getAllUsers();
    
    // Emergency contact management
    bool addEmergencyContact(int userId, const EmergencyContact& contact);
    bool updateEmergencyContact(int userId, int contactIndex, const EmergencyContact& contact);
    bool deleteEmergencyContact(int userId, int contactIndex);
    std::vector<EmergencyContact> getEmergencyContacts(int userId);
    
    // Doctor management
    bool setFamilyDoctor(int userId, const Doctor& doctor);
    Doctor getFamilyDoctor(int userId);
    
private:
    std::string m_dbPath;
    sqlite3* m_db;
    bool m_initialized;
    
    // Helper methods
    bool executeSql(const std::string& sql);
    void createTables();
};

} // namespace hms
