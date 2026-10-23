// User accounts and the university list, each kept in a singly linked list
// (proposal section 7: "User accounts are kept in a second list").
// Lookup is a linear search, O(m) (proposal section 13).
#pragma once
#include <ctime>
#include <string>

enum class Role { Student, Teacher, Staff, Employee };  // Staff = cafeteria staff; Employee = university staff
const char* roleName(Role r);
bool roleFromName(const std::string& name, Role& out);

struct User {
    std::string login;         // lower-case university email, or staff login name
    std::string universityId;  // empty for staff
    Role role;
    std::string name;
    std::string department;    // teachers: department; students: programme/department
    std::string passwordHash;  // argon2id string from libsodium
    bool verified;             // email verified (staff: always true)
    int failedLogins;
    time_t lockedUntil;
    std::string codeHash;      // hash of the pending verification code
    time_t codeExpires;
    int codeAttempts;
    User* next;
};

struct UserList {
    User* head;
    User* tail;
    int count;
};

void ulInit(UserList& list);
User* ulAppend(UserList& list);  // new zeroed node at the tail
User* ulFind(const UserList& list, const std::string& login);
void ulClear(UserList& list);

// One row of the university's list of students, teachers and staff.
struct UniEntry {
    std::string id;
    std::string email;  // lower-case; empty only for Employee rows (no university email)
    Role role;          // Student, Teacher or Employee
    std::string name;
    std::string department;
    UniEntry* next;
};

struct UniList {
    UniEntry* head;
    UniEntry* tail;
    int count;
};

void uniInit(UniList& list);
void uniClear(UniList& list);
UniEntry* uniFindById(const UniList& list, const std::string& id);
UniEntry* uniFindByEmail(const UniList& list, const std::string& email);  // lower-case; never matches a blank email

// Loads "id,email,role,name,department" CSV [CONFIRM format]. '#' lines and a
// header line starting with "id," are skipped. Returns false with
// err = "<path> line <n>: <reason>" on a bad row.
bool uniLoadCsv(UniList& list, const std::string& path, std::string& err);
