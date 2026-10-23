// users.txt: login|universityId|role|name|department|passwordHash|verified|
//            failedLogins|lockedUntil|codeHash|codeExpires|codeAttempts
// Sessions are not saved: a restart logs everyone out.
#pragma once
#include "auth/users.h"
#include <string>

bool loadUsers(UserList& users, const std::string& dataDir, std::string& err);  // missing file = no users
bool saveUsers(const UserList& users, const std::string& dataDir, std::string& err);
