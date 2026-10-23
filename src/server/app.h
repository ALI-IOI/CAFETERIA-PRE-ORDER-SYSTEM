// Shared server state. One mutex protects every data structure (ADR-001):
// each request and each timer tick holds it for its whole
// read-modify-save sequence, and emails are sent after it is released.
#pragma once
#include "auth/auth.h"
#include "core/cafeteria.h"
#include "email/email.h"
#include "email/mailer.h"
#include <mutex>
#include <string>

struct AppConfig {
    std::string emailDomain;  // empty when still [CONFIRM]
    int sessionMinutes = 480;
    int lockoutFailures = 5;
    int lockoutMinutes = 15;
    int codeMinutes = 15;
    int cookingCapacity = 100;
};

// Reads config/app.cfg. Unknown keys or bad numbers -> false with err.
bool loadAppConfig(const std::string& path, AppConfig& cfg, std::string& err);

struct AppState {
    std::mutex mu;
    Clock* clock = nullptr;
    FakeClock* fakeClock = nullptr;  // only with --fake-clock (testing/demo)
    Cafeteria caf;
    AuthState auth;
    EmailSender* email = nullptr;
    Mailer mailer;  // own mutex; lock order is always mu -> mailer.mu, never the reverse
    std::string dataDir;
    bool secureCookies = false;  // --behind-https
};

// Writes menu/settings/reservations and users. Call with the mutex held.
void saveAll(AppState& app);

// Sends and frees the list. Call WITHOUT the mutex.
void sendAll(AppState& app, NotificationList& out);
