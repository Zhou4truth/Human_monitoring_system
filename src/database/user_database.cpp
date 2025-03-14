#include "database/user_database.hpp"
#include <iostream>
#include <sstream>

namespace hms {

UserDatabase::UserDatabase(const std::string& dbPath)
    : m_dbPath(dbPath), m_db(nullptr), m_initialized(false) {
}

UserDatabase::~UserDatabase() {
    if (m_db) {
        sqlite3_close(m_db);
    }
}

bool UserDatabase::initialize() {
    if (m_initialized) {
        return true;
    }
    
    int rc = sqlite3_open(m_dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }
    
    createTables();
    m_initialized = true;
    return true;
}

bool UserDatabase::isInitialized() const {
    return m_initialized;
}

void UserDatabase::createTables() {
    // Create users table
    std::string sql = "CREATE TABLE IF NOT EXISTS users ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "name TEXT NOT NULL,"
                      "notes TEXT,"
                      "image_reference TEXT"
                      ");";
    executeSql(sql);
    
    // Create emergency contacts table
    sql = "CREATE TABLE IF NOT EXISTS emergency_contacts ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "user_id INTEGER NOT NULL,"
          "name TEXT NOT NULL,"
          "phone TEXT NOT NULL,"
          "email TEXT,"
          "address TEXT,"
          "relationship TEXT,"
          "FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE"
          ");";
    executeSql(sql);
    
    // Create doctors table
    sql = "CREATE TABLE IF NOT EXISTS doctors ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "user_id INTEGER NOT NULL,"
          "name TEXT NOT NULL,"
          "phone TEXT NOT NULL,"
          "email TEXT,"
          "address TEXT,"
          "specialization TEXT,"
          "FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE"
          ");";
    executeSql(sql);
    
    // Enable foreign keys
    executeSql("PRAGMA foreign_keys = ON;");
}

bool UserDatabase::executeSql(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool UserDatabase::addUser(User& user) {
    if (!m_initialized && !initialize()) {
        return false;
    }
    
    std::string sql = "INSERT INTO users (name, notes, image_reference) "
                      "VALUES (?, ?, ?);";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, user.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user.notes.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user.imageReference.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        std::cerr << "SQL step error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    // Get the last inserted row ID
    user.id = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    
    // Add emergency contacts
    for (const auto& contact : user.emergencyContacts) {
        addEmergencyContact(user.id, contact);
    }
    
    // Add family doctor
    if (!user.familyDoctor.name.empty()) {
        setFamilyDoctor(user.id, user.familyDoctor);
    }
    
    return true;
}

bool UserDatabase::updateUser(const User& user) {
    if (!m_initialized && !initialize()) {
        return false;
    }
    
    std::string sql = "UPDATE users SET name = ?, notes = ?, image_reference = ? "
                      "WHERE id = ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, user.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user.notes.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user.imageReference.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, user.id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        std::cerr << "SQL step error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    return true;
}

bool UserDatabase::deleteUser(int userId) {
    if (!m_initialized && !initialize()) {
        return false;
    }
    
    // With foreign key constraints, deleting the user will cascade to contacts and doctor
    std::string sql = "DELETE FROM users WHERE id = ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, userId);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        std::cerr << "SQL step error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    return true;
}

User UserDatabase::getUserById(int userId) {
    User user;
    user.id = -1;  // Default invalid ID
    
    if (!m_initialized && !initialize()) {
        return user;
    }
    
    std::string sql = "SELECT id, name, notes, image_reference FROM users WHERE id = ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return user;
    }
    
    sqlite3_bind_int(stmt, 1, userId);
    
    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        user.id = sqlite3_column_int(stmt, 0);
        
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name) user.name = name;
        
        const char* notes = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (notes) user.notes = notes;
        
        const char* imageRef = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (imageRef) user.imageReference = imageRef;
    }
    
    sqlite3_finalize(stmt);
    
    if (user.id != -1) {
        // Get emergency contacts
        user.emergencyContacts = getEmergencyContacts(userId);
        
        // Get family doctor
        user.familyDoctor = getFamilyDoctor(userId);
    }
    
    return user;
}

std::vector<User> UserDatabase::getAllUsers() {
    std::vector<User> users;
    
    if (!m_initialized && !initialize()) {
        return users;
    }
    
    std::string sql = "SELECT id, name, notes, image_reference FROM users;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return users;
    }
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        User user;
        user.id = sqlite3_column_int(stmt, 0);
        
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name) user.name = name;
        
        const char* notes = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (notes) user.notes = notes;
        
        const char* imageRef = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (imageRef) user.imageReference = imageRef;
        
        // Get emergency contacts
        user.emergencyContacts = getEmergencyContacts(user.id);
        
        // Get family doctor
        user.familyDoctor = getFamilyDoctor(user.id);
        
        users.push_back(user);
    }
    
    sqlite3_finalize(stmt);
    
    return users;
}

bool UserDatabase::addEmergencyContact(int userId, const EmergencyContact& contact) {
    if (!m_initialized && !initialize()) {
        return false;
    }
    
    std::string sql = "INSERT INTO emergency_contacts (user_id, name, phone, email, address, relationship) "
                      "VALUES (?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_text(stmt, 2, contact.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, contact.phone.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, contact.email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, contact.address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, contact.relationship.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        std::cerr << "SQL step error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    return true;
}

bool UserDatabase::updateEmergencyContact(int userId, int contactIndex, const EmergencyContact& contact) {
    if (!m_initialized && !initialize()) {
        return false;
    }
    
    // First, get all contacts for this user
    std::vector<EmergencyContact> contacts = getEmergencyContacts(userId);
    
    if (contactIndex < 0 || contactIndex >= static_cast<int>(contacts.size())) {
        std::cerr << "Invalid contact index: " << contactIndex << std::endl;
        return false;
    }
    
    // Get the contact ID from the database
    std::string sql = "SELECT id FROM emergency_contacts WHERE user_id = ? LIMIT 1 OFFSET ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_int(stmt, 2, contactIndex);
    
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_ROW) {
        std::cerr << "Contact not found at index: " << contactIndex << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    
    int contactId = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    
    // Now update the contact
    sql = "UPDATE emergency_contacts SET name = ?, phone = ?, email = ?, address = ?, relationship = ? "
          "WHERE id = ?;";
    
    rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, contact.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, contact.phone.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, contact.email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, contact.address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, contact.relationship.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, contactId);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        std::cerr << "SQL step error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    return true;
}

bool UserDatabase::deleteEmergencyContact(int userId, int contactIndex) {
    if (!m_initialized && !initialize()) {
        return false;
    }
    
    // First, get all contacts for this user
    std::vector<EmergencyContact> contacts = getEmergencyContacts(userId);
    
    if (contactIndex < 0 || contactIndex >= static_cast<int>(contacts.size())) {
        std::cerr << "Invalid contact index: " << contactIndex << std::endl;
        return false;
    }
    
    // Get the contact ID from the database
    std::string sql = "SELECT id FROM emergency_contacts WHERE user_id = ? LIMIT 1 OFFSET ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_int(stmt, 2, contactIndex);
    
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_ROW) {
        std::cerr << "Contact not found at index: " << contactIndex << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    
    int contactId = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    
    // Now delete the contact
    sql = "DELETE FROM emergency_contacts WHERE id = ?;";
    
    rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, contactId);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        std::cerr << "SQL step error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    return true;
}

std::vector<EmergencyContact> UserDatabase::getEmergencyContacts(int userId) {
    std::vector<EmergencyContact> contacts;
    
    if (!m_initialized && !initialize()) {
        return contacts;
    }
    
    std::string sql = "SELECT name, phone, email, address, relationship FROM emergency_contacts "
                      "WHERE user_id = ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return contacts;
    }
    
    sqlite3_bind_int(stmt, 1, userId);
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        EmergencyContact contact;
        
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name) contact.name = name;
        
        const char* phone = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (phone) contact.phone = phone;
        
        const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (email) contact.email = email;
        
        const char* address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (address) contact.address = address;
        
        const char* relationship = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (relationship) contact.relationship = relationship;
        
        contacts.push_back(contact);
    }
    
    sqlite3_finalize(stmt);
    
    return contacts;
}

bool UserDatabase::setFamilyDoctor(int userId, const Doctor& doctor) {
    if (!m_initialized && !initialize()) {
        return false;
    }
    
    // First, check if a doctor already exists for this user
    std::string sql = "SELECT id FROM doctors WHERE user_id = ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, userId);
    
    rc = sqlite3_step(stmt);
    bool doctorExists = (rc == SQLITE_ROW);
    int doctorId = doctorExists ? sqlite3_column_int(stmt, 0) : -1;
    
    sqlite3_finalize(stmt);
    
    if (doctorExists) {
        // Update existing doctor
        sql = "UPDATE doctors SET name = ?, phone = ?, email = ?, address = ?, specialization = ? "
              "WHERE id = ?;";
        
        rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, doctor.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, doctor.phone.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, doctor.email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, doctor.address.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, doctor.specialization.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 6, doctorId);
    } else {
        // Insert new doctor
        sql = "INSERT INTO doctors (user_id, name, phone, email, address, specialization) "
              "VALUES (?, ?, ?, ?, ?, ?);";
        
        rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, userId);
        sqlite3_bind_text(stmt, 2, doctor.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, doctor.phone.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, doctor.email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, doctor.address.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, doctor.specialization.c_str(), -1, SQLITE_STATIC);
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        std::cerr << "SQL step error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    
    return true;
}

Doctor UserDatabase::getFamilyDoctor(int userId) {
    Doctor doctor;
    
    if (!m_initialized && !initialize()) {
        return doctor;
    }
    
    std::string sql = "SELECT name, phone, email, address, specialization FROM doctors "
                      "WHERE user_id = ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return doctor;
    }
    
    sqlite3_bind_int(stmt, 1, userId);
    
    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name) doctor.name = name;
        
        const char* phone = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (phone) doctor.phone = phone;
        
        const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (email) doctor.email = email;
        
        const char* address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (address) doctor.address = address;
        
        const char* specialization = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (specialization) doctor.specialization = specialization;
    }
    
    sqlite3_finalize(stmt);
    
    return doctor;
}

} // namespace hms
