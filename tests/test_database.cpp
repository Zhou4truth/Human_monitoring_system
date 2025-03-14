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
    user.notes = "Test user notes";
    user.imageReference = "/path/to/test/image.jpg";
    
    // Add user
    bool added = db.addUser(user);
    assert(added && "Failed to add user");
    assert(user.id > 0 && "User ID not set after adding");
    
    // Retrieve user
    User retrievedUser = db.getUserById(user.id);
    assert(retrievedUser.id == user.id && "Retrieved user ID doesn't match");
    assert(retrievedUser.name == user.name && "Retrieved user name doesn't match");
    assert(retrievedUser.notes == user.notes && "Retrieved user notes don't match");
    
    // Update user
    retrievedUser.name = "Updated Test User";
    retrievedUser.notes = "Updated notes";
    bool updated = db.updateUser(retrievedUser);
    assert(updated && "Failed to update user");
    
    // Verify update
    User updatedUser = db.getUserById(user.id);
    assert(updatedUser.name == "Updated Test User" && "User name not updated");
    assert(updatedUser.notes == "Updated notes" && "User notes not updated");
    
    // Delete user
    bool deleted = db.deleteUser(user.id);
    assert(deleted && "Failed to delete user");
    
    // Verify deletion
    User deletedUser = db.getUserById(user.id);
    assert(deletedUser.id == 0 && "User not properly deleted");
    
    std::cout << "User CRUD operations test completed successfully" << std::endl;
}

// Test function to verify emergency contact functionality
void test_emergency_contacts() {
    std::cout << "Testing emergency contact operations..." << std::endl;
    
    // Initialize database with in-memory SQLite
    UserDatabase db(":memory:");
    bool initialized = db.initialize();
    assert(initialized && "Database initialization failed");
    
    // Create a test user
    User user;
    user.name = "Test User";
    user.notes = "Test user notes";
    
    // Add user
    bool added = db.addUser(user);
    assert(added && "Failed to add user");
    
    // Create test emergency contacts
    EmergencyContact contact1;
    contact1.name = "Emergency Contact 1";
    contact1.phone = "555-111-2222";
    contact1.email = "contact1@example.com";
    contact1.address = "456 Contact St";
    contact1.relationship = "Spouse";
    
    EmergencyContact contact2;
    contact2.name = "Emergency Contact 2";
    contact2.phone = "555-333-4444";
    contact2.email = "contact2@example.com";
    contact2.address = "789 Contact Ave";
    contact2.relationship = "Child";
    
    // Add emergency contacts
    bool contact1Added = db.addEmergencyContact(user.id, contact1);
    bool contact2Added = db.addEmergencyContact(user.id, contact2);
    assert(contact1Added && contact2Added && "Failed to add emergency contacts");
    
    // Retrieve emergency contacts
    std::vector<EmergencyContact> contacts = db.getEmergencyContacts(user.id);
    assert(contacts.size() == 2 && "Wrong number of emergency contacts retrieved");
    assert(contacts[0].name == "Emergency Contact 1" && "First contact name doesn't match");
    assert(contacts[1].name == "Emergency Contact 2" && "Second contact name doesn't match");
    
    // Update an emergency contact
    contacts[0].phone = "555-999-8888";
    bool contactUpdated = db.updateEmergencyContact(user.id, 0, contacts[0]);
    assert(contactUpdated && "Failed to update emergency contact");
    
    // Verify update
    contacts = db.getEmergencyContacts(user.id);
    assert(contacts[0].phone == "555-999-8888" && "Emergency contact not updated");
    
    // Delete an emergency contact
    bool contactDeleted = db.deleteEmergencyContact(user.id, 1);
    assert(contactDeleted && "Failed to delete emergency contact");
    
    // Verify deletion
    contacts = db.getEmergencyContacts(user.id);
    assert(contacts.size() == 1 && "Emergency contact not deleted");
    
    std::cout << "Emergency contact operations test completed successfully" << std::endl;
}

// Test function to verify family doctor functionality
void test_family_doctors() {
    std::cout << "Testing family doctor operations..." << std::endl;
    
    // Initialize database with in-memory SQLite
    UserDatabase db(":memory:");
    bool initialized = db.initialize();
    assert(initialized && "Database initialization failed");
    
    // Create a test user
    User user;
    user.name = "Test User";
    
    // Add user
    bool added = db.addUser(user);
    assert(added && "Failed to add user");
    
    // Create test doctor
    Doctor doctor;
    doctor.name = "Dr. Smith";
    doctor.phone = "555-777-8888";
    doctor.email = "drsmith@example.com";
    doctor.address = "123 Medical Center";
    doctor.specialization = "Geriatrics";
    
    // Set family doctor
    bool doctorSet = db.setFamilyDoctor(user.id, doctor);
    assert(doctorSet && "Failed to set family doctor");
    
    // Retrieve family doctor
    Doctor retrievedDoctor = db.getFamilyDoctor(user.id);
    assert(retrievedDoctor.name == "Dr. Smith" && "Doctor name doesn't match");
    assert(retrievedDoctor.specialization == "Geriatrics" && "Doctor specialization doesn't match");
    
    // Update doctor
    doctor.name = "Dr. Johnson";
    doctor.specialization = "Internal Medicine";
    doctorSet = db.setFamilyDoctor(user.id, doctor);
    assert(doctorSet && "Failed to update family doctor");
    
    // Verify update
    retrievedDoctor = db.getFamilyDoctor(user.id);
    assert(retrievedDoctor.name == "Dr. Johnson" && "Doctor name not updated");
    assert(retrievedDoctor.specialization == "Internal Medicine" && "Doctor specialization not updated");
    
    std::cout << "Family doctor operations test completed successfully" << std::endl;
}

int main() {
    std::cout << "Starting Database tests..." << std::endl;
    
    try {
        test_user_crud();
        test_emergency_contacts();
        test_family_doctors();
        
        std::cout << "All Database tests completed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
