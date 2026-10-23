#include "auth/users.h"
#include "auth/validation.h"
#include "storage/file_util.h"
#include <fstream>

const char* roleName(Role r) {
    switch (r) {
        case Role::Student: return "student";
        case Role::Teacher: return "teacher";
        case Role::Staff: return "staff";
        case Role::Employee: return "employee";
    }
    return "unknown";
}

bool roleFromName(const std::string& name, Role& out) {
    if (name == "student") out = Role::Student;
    else if (name == "teacher") out = Role::Teacher;
    else if (name == "staff") out = Role::Staff;
    else if (name == "employee") out = Role::Employee;
    else return false;
    return true;
}

void ulInit(UserList& list) {
    list.head = nullptr;
    list.tail = nullptr;
    list.count = 0;
}

User* ulAppend(UserList& list) {
    User* u = new User;
    u->role = Role::Student;
    u->verified = false;
    u->failedLogins = 0;
    u->lockedUntil = 0;
    u->codeExpires = 0;
    u->codeAttempts = 0;
    u->next = nullptr;
    if (list.tail == nullptr) list.head = u;
    else list.tail->next = u;
    list.tail = u;
    list.count++;
    return u;
}

User* ulFind(const UserList& list, const std::string& login) {
    for (User* u = list.head; u != nullptr; u = u->next) {
        if (u->login == login) return u;
    }
    return nullptr;
}

void ulClear(UserList& list) {
    User* u = list.head;
    while (u != nullptr) {
        User* next = u->next;
        delete u;
        u = next;
    }
    ulInit(list);
}

void uniInit(UniList& list) {
    list.head = nullptr;
    list.tail = nullptr;
    list.count = 0;
}

void uniClear(UniList& list) {
    UniEntry* e = list.head;
    while (e != nullptr) {
        UniEntry* next = e->next;
        delete e;
        e = next;
    }
    uniInit(list);
}

UniEntry* uniFindById(const UniList& list, const std::string& id) {
    for (UniEntry* e = list.head; e != nullptr; e = e->next) {
        if (e->id == id) return e;
    }
    return nullptr;
}

UniEntry* uniFindByEmail(const UniList& list, const std::string& email) {
    for (UniEntry* e = list.head; e != nullptr; e = e->next)
        if (!e->email.empty() && e->email == email) return e;
    return nullptr;
}

bool uniLoadCsv(UniList& list, const std::string& path, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "cannot open university list " + path;
        return false;
    }
    std::string line, f[5];
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line.rfind("id,", 0) == 0) continue;
        Role role = Role::Student;
        int n = splitFields(line, ',', f, 5);
        bool roleOk = n == 5 && roleFromName(f[2], role) && role != Role::Staff;
        // Only university staff may have no university email (the admin tool
        // then creates their account with a personal one).
        bool emailOk = roleOk && (f[1].empty() ? role == Role::Employee : validEmail(f[1]));
        if (!roleOk || !emailOk || !validUniversityId(f[0]) || !validName(f[3]) ||
            (!f[4].empty() && !validName(f[4]))) {
            err = path + " line " + std::to_string(lineNo) +
                  ": expected id,email,student|teacher|employee,name,department";
            return false;
        }
        if (uniFindById(list, f[0]) != nullptr) {
            err = path + " line " + std::to_string(lineNo) + ": duplicate id";
            return false;
        }
        if (!f[1].empty() && uniFindByEmail(list, toLower(f[1])) != nullptr) {
            err = path + " line " + std::to_string(lineNo) + ": duplicate email";
            return false;
        }
        UniEntry* e = new UniEntry;
        e->id = f[0];
        e->email = toLower(f[1]);
        e->role = role;
        e->name = f[3];
        e->department = f[4];
        e->next = nullptr;
        if (list.tail == nullptr) list.head = e;
        else list.tail->next = e;
        list.tail = e;
        list.count++;
    }
    return true;
}
