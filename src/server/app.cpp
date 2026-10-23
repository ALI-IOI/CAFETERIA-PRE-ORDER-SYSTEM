#include "server/app.h"
#include "auth/user_store.h"
#include "storage/file_util.h"
#include "storage/storage.h"
#include <cstdio>
#include <fstream>

bool loadAppConfig(const std::string& path, AppConfig& cfg, std::string& err) {
    std::ifstream in(path);
    if (!in) return true;  // defaults; registration stays disabled
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        std::string key = eq == std::string::npos ? line : line.substr(0, eq);
        std::string value = eq == std::string::npos ? "" : line.substr(eq + 1);
        int n = 0;
        bool isNumber = parseIntStrict(value, n) && n > 0;
        if (key == "email_domain") {
            cfg.emailDomain = value.find("[CONFIRM") == std::string::npos ? value : "";
        } else if (key == "session_minutes" && isNumber) cfg.sessionMinutes = n;
        else if (key == "lockout_failures" && isNumber) cfg.lockoutFailures = n;
        else if (key == "lockout_minutes" && isNumber) cfg.lockoutMinutes = n;
        else if (key == "code_minutes" && isNumber) cfg.codeMinutes = n;
        else if (key == "cooking_capacity" && isNumber && n <= 10000) cfg.cookingCapacity = n;
        else {
            err = path + " line " + std::to_string(lineNo) + ": unknown key or bad value";
            return false;
        }
    }
    return true;
}

void saveAll(AppState& app) {
    std::string err;
    if (!saveCafeteria(app.caf, app.dataDir, err)) std::fprintf(stderr, "SAVE FAILED: %s\n", err.c_str());
    if (!saveUsers(app.auth.users, app.dataDir, err)) std::fprintf(stderr, "SAVE FAILED: %s\n", err.c_str());
}

void sendAll(AppState& app, NotificationList& out) {
    mailerSendAll(app.mailer, out);  // failures wait in the retry queue (addition #2)
}
