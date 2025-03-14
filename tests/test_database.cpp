#include "database/user_database.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace hms;

// Test function to verify user database functionality
void test_user_crud() {
    std::cout << "Testing User CRUD operations..." << std::endl;
    
    // Initialize database with in-memory SQLite
    UserDatabase db(":memory:");
    bool initialized = db.initialize();
    assert(initialized && "Database initialization failed");
    
    // Create a test user
    User user;
    user.name = "Test User";
    user.age = 65;
    user.address = "123 Test Street";
    user.phoneNumber = "555-123-4567";
    user.email = "test@example.com";
    
    // Add user
    bool added = db.addUser(user);
    assert(added && "Failed to add user");
    assert(user.id > 0 && "User ID not set after adding");
    
    // Retrieve user
    User retrievedUser = db.getUserById(user.id);
    assert(retrievedUser.id == user.id && "Retrieved user ID doesn't match");
    assert(retrievedUser.name == user.name && "Retrieved user name doesn't match");
    assert(retrievedUser.age == user.age && "Retrieved user age doesn't match");
    
    // Update user
    retrievedUser.name = "Updated Test User";
    retrievedUser.age = 66;
    bool updated = db.updateUser(retrievedUser);
    assert(updated && "Failed to update user");
    
    // Verify update
    User updatedUser = db.getUserById(user.id);
    assert(updatedUser.name == "Updated Test User" && "User name not updated");
    assert(updatedUser.age == 66 && "User age not updated");
    
    // Delete user
    bool deleted = db.deleteUser(user.id);
    assert(deleted && "Failed to delete user");
    
    // Verify deletion
    User deletedUser = db.getUserById(user.id);
    assert(deletedUser.id == -1 && "User not properly deleted");
    
    std::cout << "User CRUD tests passed!" << std::endl;
}

// Test function to verify emergency contact functionality
void test_emergency_contacts() {
    std::cout << "Testing Emergency Contact operations..." << std::endl;
    
    // Initialize database with in-memory SQLite
    UserDatabase db(":memory:");
    db.initialize();
    
    // Create a test user
    User user;
    user.name = "Test User";
    user.age = 65;
    db.addUser(user);
    
    // Create emergency contacts
    EmergencyContact contact1;
    contact1.name = "Emergency Contact 1";
    contact1.phoneNumber = "555-111-2222";
    contact1.email = "emergency1@example.com";
    contact1.relationship = "Spouse";
    
    EmergencyContact contact2;
    contact2.name = "Emergency Contact 2";
    contact2.phoneNumber = "555-333-4444";
    contact2.email = "emergency2@example.com";
    contact2.relationship = "Child";
    
    // Add emergency contacts
    bool added1 = db.addEmergencyContact(user.id, contact1);
    bool added2 = db.addEmergencyContact(user.id, contact2);
    assert(added1 && added2 && "Failed to add emergency contacts");
    
    // Get emergency contacts
    std::vector<EmergencyContact> contacts = db.getEmergencyContacts(user.id);
    assert(contacts.size() == 2 && "Incorrect number of emergency contacts");
    
    // Update emergency contact
    contacts[0].phoneNumber = "555-999-8888";
    bool updated = db.updateEmergencyContact(user.id, contacts[0]);
    assert(updated && "Failed to update emergency contact");
    
    // Verify update
    contacts = db.getEmergencyContacts(user.id);
    assert(contacts[0].phoneNumber == "555-999-8888" && "Emergency contact not updated");
    
    // Delete emergency contact
    bool deleted = db.deleteEmergencyContact(user.id, contacts[0].id);
    assert(deleted && "Failed to delete emergency contact");
    
    // Verify deletion
    contacts = db.getEmergencyContacts(user.id);
    assert(contacts.size() == 1 && "Emergency contact not deleted");
    
    std::cout << "Emergency Contact tests passed!" << std::endl;
}

// Test function to verify family doctor functionality
void test_family_doctors() {
    std::cout << "Testing Family Doctor operations..." << std::endl;
    
    // Initialize database with in-memory SQLite
    UserDatabase db(":memory:");
    db.initialize();
    
    // Create a test user
    User user;
    user.name = "Test User";
    user.age = 65;
    db.addUser(user);
    
    // Create family doctor
    FamilyDoctor doctor;
    doctor.name = "Dr. Test";
    doctor.phoneNumber = "555-777-8888";
    doctor.email = "doctor@example.com";
    doctor.hospital = "Test Hospital";
    
    // Add family doctor
    bool added = db.addFamilyDoctor(user.id, doctor);
    assert(added && "Failed to add family doctor");
    
    // Get family doctor
    FamilyDoctor retrievedDoctor = db.getFamilyDoctor(user.id);
    assert(retrievedDoctor.name == doctor.name && "Retrieved doctor name doesn't match");
    
    // Update family doctor
    retrievedDoctor.hospital = "Updated Hospital";
    bool updated = db.updateFamilyDoctor(user.id, retrievedDoctor);
    assert(updated && "Failed to update family doctor");
    
    // Verify update
    retrievedDoctor = db.getFamilyDoctor(user.id);
    assert(retrievedDoctor.hospital == "Updated Hospital" && "Family doctor not updated");
    
    // Delete family doctor
    bool deleted = db.deleteFamilyDoctor(user.id);
    assert(deleted && "Failed to delete family doctor");
    
    // Verify deletion
    retrievedDoctor = db.getFamilyDoctor(user.id);
    assert(retrievedDoctor.id == -1 && "Family doctor not deleted");
    
    std::cout << "Family Doctor tests passed!" << std::endl;
}

int main() {
    std::cout << "Starting User Database tests..." << std::endl;
    
    try {
        test_user_crud();
        test_emergency_contacts();
        test_family_doctors();
        
        std::cout << "All User Database tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
