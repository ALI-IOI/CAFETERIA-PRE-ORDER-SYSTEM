// Registration, email verification, login, sessions and lockout
// (proposal sections 5 and 10). Callers must hold the server mutex.
#pragma once
#include "auth/users.h"
#include "core/clock.h"
#include "core/notification.h"
#include <string>

struct AuthConfig {
    std::string emailDomain;  // [CONFIRM] university domain; empty = registration disabled
    int sessionMinutes;       // [CONFIRM] 480
    int lockoutFailures;      // [CONFIRM] 5
    int lockoutMinutes;       // [CONFIRM] 15
    int codeMinutes;          // [CONFIRM] 15
    int codeMaxAttempts;      // 5
};
AuthConfig authDefaults();

struct Session {
    std::string token;  // 32 random bytes, hex
    std::string login;
    Role role;
    time_t expiresAt;
    Session* next;
};

// Linked list of open sessions.
struct SessionList {
    Session* head;
    int count;
};

struct AuthState {
    UserList users;
    UniList uni;
    SessionList sessions;
    AuthConfig cfg;
    Clock* clock;
};

enum class AuthResult {
    Ok,
    NotUniversityEmail,
    IdNotListed,
    EmailIdMismatch,
    AlreadyRegistered,
    WeakPassword,
    BadCode,
    CodeExpired,
    BadCredentials,
    Locked,
    NotVerified,
    InvalidInput,
    CryptoError,
};
const char* authMessage(AuthResult r);

void authInit(AuthState& a, Clock* clock, const AuthConfig& cfg);
void authFree(AuthState& a);

// Step 1: university email + listed ID + password. The role, name and
// department are copied from the university list, never from the user.
// Queues an email with a 6-digit code. Re-running it for an unverified
// account replaces the password and sends a new code.
AuthResult registerStart(AuthState& a, const std::string& email, const std::string& universityId,
                         const std::string& password, NotificationList& out);

// Step 2: the code from the email plus the password chosen in step 1. Asking
// for the password again means a stranger who re-registers the same email
// (replacing the pending password) cannot get their password verified by the
// real owner entering the code. Wrong code/password 5 times or after 15
// minutes -> CodeExpired.
AuthResult registerVerify(AuthState& a, const std::string& email, const std::string& code,
                          const std::string& password);

// Staff can log in only through the staff portal (staffPortal = true) and
// students/teachers only through the public page. Repeated failures lock the
// account for a while.
AuthResult login(AuthState& a, const std::string& login, const std::string& password, bool staffPortal,
                 std::string& tokenOut);

// nullptr for unknown or expired tokens (expired ones are removed).
Session* sessionGet(AuthState& a, const std::string& token);
void logout(AuthState& a, const std::string& token);
void sessionPruneExpired(AuthState& a);

// Administrator only (cafeteria_admin tool); there is no web route for this.
AuthResult addStaff(AuthState& a, const std::string& login, const std::string& name,
                    const std::string& password);
