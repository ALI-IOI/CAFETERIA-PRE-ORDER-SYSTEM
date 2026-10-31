// Registration, email verification, login, sessions and lockout
// (proposal sections 5 and 10). Callers must hold the server mutex, except for
// makeRegisterHashes and the password check between loginBegin and loginFinish.
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

const int CODE_RESEND_SECONDS = 60;    // a new verification code at most once a minute per email
const int MAX_SESSIONS_PER_USER = 10;  // the 11th login signs out that person's oldest session

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
    NoUniversityEmail,  // university staff row without an email: the admin tool creates the account
    CodeRecentlySent,   // a verification code went out under CODE_RESEND_SECONDS ago: nothing sent or changed
    NotEligible,        // add-person: the ID is not a university staff row without an email
};
const char* authMessage(AuthResult r);

void authInit(AuthState& a, Clock* clock, const AuthConfig& cfg);
void authFree(AuthState& a);

// Step 1: university email + listed ID + password. The role, name and
// department are copied from the university list, never from the user.
// Queues an email with a 6-digit code. Re-running it for an unverified
// account replaces the password and sends a new code, at most once per
// CODE_RESEND_SECONDS.
// Within CODE_RESEND_SECONDS of the last code: CodeRecentlySent, nothing sent or
// changed. The server runs it in three steps so the slow hashing (Argon2, 0.1-0.3 s)
// happens while its lock is free: registerCheck (lock held), makeRegisterHashes (no
// lock), then registerStart with those hashes (lock held), which repeats every check.
// With pre = nullptr it hashes itself (the one-call form, for tests and tools).
struct RegisterHashes {
    std::string code, codeHash, passwordHash;
};
AuthResult registerCheck(AuthState& a, const std::string& email, const std::string& universityId,
                         const std::string& password);
bool makeRegisterHashes(const std::string& password, RegisterHashes& h);  // false: crypto error
AuthResult registerStart(AuthState& a, const std::string& email, const std::string& universityId,
                         const std::string& password, NotificationList& out, const RegisterHashes* pre = nullptr);

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

// login in three steps, for the server: loginBegin (lock held), the password check
// verifyPassword(t.hash, password) with the lock free, then loginFinish (lock held).
// loginBegin counts the attempt as failed before the check, so guesses sent at the
// same time cannot slip past the lockout; loginFinish finds the account again by
// name. Unknown names get a dummy hash so they take as long as real ones.
struct LoginTicket {
    std::string login;  // empty: no such account (the check runs anyway, then fails)
    std::string hash;   // the stored hash when the attempt began
};
AuthResult loginBegin(AuthState& a, const std::string& login, const std::string& password, bool staffPortal,
                      LoginTicket& t);
AuthResult loginFinish(AuthState& a, const LoginTicket& t, bool passwordOk, std::string& tokenOut);

// My account: needs the current password (a wrong one counts toward the same
// lockout as logins). The new one must pass validPassword and differ from the
// current one. On success every other session of this account ends; the
// session `keepToken` stays signed in.
AuthResult changePassword(AuthState& a, const std::string& login, const std::string& current,
                          const std::string& next, const std::string& keepToken);

// nullptr for unknown or expired tokens (expired ones are removed).
Session* sessionGet(AuthState& a, const std::string& token);
void logout(AuthState& a, const std::string& token);
void sessionPruneExpired(AuthState& a);

// Administrator only (cafeteria_admin add-person): an account for university
// staff who have no university email. The ID must be an employee row with a
// blank email; `email` is any valid address (lower-cased) not used by another
// account or list row. Role, name and department come from the list. Active at once.
AuthResult addPerson(AuthState& a, const std::string& universityId, const std::string& email,
                     const std::string& password);

// Administrator only (cafeteria_admin tool); there is no web route for this.
AuthResult addStaff(AuthState& a, const std::string& login, const std::string& name,
                    const std::string& password);
