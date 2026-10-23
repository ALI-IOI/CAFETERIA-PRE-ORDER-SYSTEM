#include "auth/user_store.h"
#include "storage/file_util.h"
#include <fstream>

bool loadUsers(UserList& users, const std::string& dataDir, std::string& err) {
    std::ifstream in(dataDir + "/users.txt");
    if (!in) return true;
    std::string line, f[12];
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        Role role;
        int verified, failed, attempts;
        long long lockedUntil, codeExpires;
        if (splitFields(line, '|', f, 12) != 12 || f[0].empty() || !roleFromName(f[2], role) ||
            !parseIntStrict(f[6], verified) || !parseIntStrict(f[7], failed) ||
            !parseIntStrict(f[8], lockedUntil) || !parseIntStrict(f[10], codeExpires) ||
            !parseIntStrict(f[11], attempts) || ulFind(users, f[0]) != nullptr) {
            err = "users.txt line " + std::to_string(lineNo) + ": bad record";
            return false;
        }
        User* u = ulAppend(users);
        u->login = f[0];
        u->universityId = f[1];
        u->role = role;
        u->name = f[3];
        u->department = f[4];
        u->passwordHash = f[5];
        u->verified = verified != 0;
        u->failedLogins = failed;
        u->lockedUntil = (time_t)lockedUntil;
        u->codeHash = f[9];
        u->codeExpires = (time_t)codeExpires;
        u->codeAttempts = attempts;
    }
    return true;
}

bool saveUsers(const UserList& users, const std::string& dataDir, std::string& err) {
    if (!ensureDir(dataDir, err)) return false;
    std::string s =
        "# login|universityId|role|name|department|passwordHash|verified|failedLogins|lockedUntil|"
        "codeHash|codeExpires|codeAttempts\n";
    for (User* u = users.head; u != nullptr; u = u->next) {
        s += u->login + "|" + u->universityId + "|" + roleName(u->role) + "|" + u->name + "|" + u->department +
             "|" + u->passwordHash + "|" + (u->verified ? "1" : "0") + "|" + std::to_string(u->failedLogins) +
             "|" + std::to_string((long long)u->lockedUntil) + "|" + u->codeHash + "|" +
             std::to_string((long long)u->codeExpires) + "|" + std::to_string(u->codeAttempts) + "\n";
    }
    return atomicWrite(dataDir + "/users.txt", s, err);
}
