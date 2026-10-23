#include "auth/auth.h"
#include "auth/crypto.h"
#include "auth/validation.h"

AuthConfig authDefaults() {
    AuthConfig c;
    c.emailDomain = "";
    c.sessionMinutes = 8 * 60;
    c.lockoutFailures = 5;
    c.lockoutMinutes = 15;
    c.codeMinutes = 15;
    c.codeMaxAttempts = 5;
    return c;
}

const char* authMessage(AuthResult r) {
    switch (r) {
        case AuthResult::Ok: return "OK";
        case AuthResult::NotUniversityEmail: return "Please use your university email address.";
        case AuthResult::IdNotListed: return "That ID is not on the university's list.";
        case AuthResult::EmailIdMismatch: return "That email does not match the university record for this ID.";
        case AuthResult::AlreadyRegistered: return "An account already exists for this email.";
        case AuthResult::WeakPassword: return "Password must be 8 to 128 characters.";
        case AuthResult::BadCode:
            return "That code is not correct, or the password differs from the one you chose. "
                   "If someone else registered your email, simply register again.";
        case AuthResult::CodeExpired: return "The verification code has expired. Please register again to get a new code.";
        case AuthResult::BadCredentials: return "Wrong login or password.";
        case AuthResult::Locked: return "Too many failed attempts. The account is locked for a short time.";
        case AuthResult::NotVerified: return "Please verify your email first.";
        case AuthResult::InvalidInput: return "Invalid input.";
        case AuthResult::CryptoError: return "Internal error.";
        case AuthResult::NoUniversityEmail:
            return "You have no university email. Ask the cafeteria office to create your account.";
        case AuthResult::NotEligible: return "That ID is not a university staff member without a university email.";
    }
    return "Unknown error.";
}

void authInit(AuthState& a, Clock* clock, const AuthConfig& cfg) {
    ulInit(a.users);
    uniInit(a.uni);
    a.sessions.head = nullptr;
    a.sessions.count = 0;
    a.cfg = cfg;
    a.clock = clock;
    cryptoInit();
}

static void clearSessions(SessionList& s) {
    Session* cur = s.head;
    while (cur != nullptr) {
        Session* next = cur->next;
        delete cur;
        cur = next;
    }
    s.head = nullptr;
    s.count = 0;
}

void authFree(AuthState& a) {
    ulClear(a.users);
    uniClear(a.uni);
    clearSessions(a.sessions);
}

AuthResult registerStart(AuthState& a, const std::string& emailIn, const std::string& universityId,
                         const std::string& password, NotificationList& out) {
    std::string email = toLower(emailIn);
    UniEntry* entry = validUniversityId(universityId) ? uniFindById(a.uni, universityId) : nullptr;
    // Staff without a university email will type a personal one: tell them where to go
    // before the domain check, whatever they typed.
    if (entry != nullptr && entry->email.empty()) return AuthResult::NoUniversityEmail;
    if (a.cfg.emailDomain.empty() || !validEmail(email) || emailDomain(email) != toLower(a.cfg.emailDomain))
        return AuthResult::NotUniversityEmail;
    if (entry == nullptr) return AuthResult::IdNotListed;
    if (entry->email != email) return AuthResult::EmailIdMismatch;
    if (!validPassword(password)) return AuthResult::WeakPassword;

    User* u = ulFind(a.users, email);
    if (u != nullptr && u->verified) return AuthResult::AlreadyRegistered;

    std::string code = randomDigits(6);
    std::string pwHash = hashPassword(password);
    std::string codeHash = hashPassword(code);
    if (pwHash.empty() || codeHash.empty()) return AuthResult::CryptoError;

    if (u == nullptr) u = ulAppend(a.users);
    u->login = email;
    u->universityId = entry->id;
    u->role = entry->role;
    u->name = entry->name;
    u->department = entry->department;
    u->passwordHash = pwHash;
    u->verified = false;
    u->codeHash = codeHash;
    u->codeExpires = a.clock->now() + (time_t)a.cfg.codeMinutes * 60;
    u->codeAttempts = 0;

    nlAppend(out, NotifType::VerificationCode, email, "Your cafeteria verification code",
             "Your verification code is " + code + ". It expires in " + std::to_string(a.cfg.codeMinutes) +
                 " minutes.\nIf you did not try to register, you can ignore this email.",
             0);
    return AuthResult::Ok;
}

AuthResult registerVerify(AuthState& a, const std::string& emailIn, const std::string& code,
                          const std::string& password) {
    User* u = ulFind(a.users, toLower(emailIn));
    if (u == nullptr) return AuthResult::BadCode;
    if (u->verified) return AuthResult::AlreadyRegistered;
    if (u->codeHash.empty() || a.clock->now() >= u->codeExpires || u->codeAttempts >= a.cfg.codeMaxAttempts)
        return AuthResult::CodeExpired;
    if (code.size() != 6 || password.size() > (size_t)MAX_PASSWORD_LEN || !verifyPassword(u->codeHash, code) ||
        !verifyPassword(u->passwordHash, password)) {
        u->codeAttempts++;
        return AuthResult::BadCode;
    }
    u->verified = true;
    u->codeHash.clear();
    u->codeAttempts = 0;
    return AuthResult::Ok;
}

AuthResult login(AuthState& a, const std::string& loginIn, const std::string& password, bool staffPortal,
                 std::string& tokenOut) {
    tokenOut.clear();
    if (loginIn.empty() || loginIn.size() > (size_t)MAX_EMAIL_LEN || password.size() > (size_t)MAX_PASSWORD_LEN)
        return AuthResult::BadCredentials;
    User* u = ulFind(a.users, toLower(loginIn));
    if (u == nullptr) return AuthResult::BadCredentials;
    if ((u->role == Role::Staff) != staffPortal) return AuthResult::BadCredentials;

    time_t now = a.clock->now();
    if (now < u->lockedUntil) return AuthResult::Locked;
    if (!verifyPassword(u->passwordHash, password)) {
        u->failedLogins++;
        if (u->failedLogins >= a.cfg.lockoutFailures) {
            u->failedLogins = 0;
            u->lockedUntil = now + (time_t)a.cfg.lockoutMinutes * 60;
            return AuthResult::Locked;
        }
        return AuthResult::BadCredentials;
    }
    u->failedLogins = 0;
    if (!u->verified) return AuthResult::NotVerified;

    Session* s = new Session;
    s->token = randomToken(32);
    s->login = u->login;
    s->role = u->role;
    s->expiresAt = now + (time_t)a.cfg.sessionMinutes * 60;
    s->next = a.sessions.head;  // insert at head: O(1)
    a.sessions.head = s;
    a.sessions.count++;
    tokenOut = s->token;
    return AuthResult::Ok;
}

// Unlinks and frees the session after `prev` (or the head when prev is null).
static void removeAfter(SessionList& list, Session* prev, Session* cur) {
    if (prev == nullptr) list.head = cur->next;
    else prev->next = cur->next;
    delete cur;
    list.count--;
}

Session* sessionGet(AuthState& a, const std::string& token) {
    if (token.size() != 64) return nullptr;
    time_t now = a.clock->now();
    Session* prev = nullptr;
    for (Session* s = a.sessions.head; s != nullptr; prev = s, s = s->next) {
        if (constantTimeEquals(s->token, token)) {
            if (now >= s->expiresAt) {
                removeAfter(a.sessions, prev, s);
                return nullptr;
            }
            return s;
        }
    }
    return nullptr;
}

void logout(AuthState& a, const std::string& token) {
    Session* prev = nullptr;
    for (Session* s = a.sessions.head; s != nullptr; prev = s, s = s->next) {
        if (s->token == token) {
            removeAfter(a.sessions, prev, s);
            return;
        }
    }
}

void sessionPruneExpired(AuthState& a) {
    time_t now = a.clock->now();
    Session* prev = nullptr;
    Session* s = a.sessions.head;
    while (s != nullptr) {
        Session* next = s->next;
        if (now >= s->expiresAt) removeAfter(a.sessions, prev, s);
        else prev = s;
        s = next;
    }
}

AuthResult addPerson(AuthState& a, const std::string& universityId, const std::string& emailIn,
                     const std::string& password) {
    std::string email = toLower(emailIn);
    if (!validEmail(email)) return AuthResult::InvalidInput;
    UniEntry* entry = uniFindById(a.uni, universityId);
    if (entry == nullptr) return AuthResult::IdNotListed;
    if (entry->role != Role::Employee || !entry->email.empty()) return AuthResult::NotEligible;
    if (!validPassword(password)) return AuthResult::WeakPassword;
    if (ulFind(a.users, email) != nullptr || uniFindByEmail(a.uni, email) != nullptr) return AuthResult::AlreadyRegistered;
    for (User* u = a.users.head; u != nullptr; u = u->next)
        if (u->universityId == universityId) return AuthResult::AlreadyRegistered;
    std::string hash = hashPassword(password);
    if (hash.empty()) return AuthResult::CryptoError;
    User* u = ulAppend(a.users);
    u->login = email;
    u->universityId = entry->id;
    u->role = Role::Employee;
    u->name = entry->name;
    u->department = entry->department;
    u->passwordHash = hash;
    u->verified = true;
    return AuthResult::Ok;
}

AuthResult addStaff(AuthState& a, const std::string& login, const std::string& name,
                    const std::string& password) {
    if (!validStaffLogin(login) || !validName(name)) return AuthResult::InvalidInput;
    if (!validPassword(password)) return AuthResult::WeakPassword;
    if (ulFind(a.users, login) != nullptr) return AuthResult::AlreadyRegistered;
    std::string hash = hashPassword(password);
    if (hash.empty()) return AuthResult::CryptoError;
    User* u = ulAppend(a.users);
    u->login = login;
    u->role = Role::Staff;
    u->name = name;
    u->passwordHash = hash;
    u->verified = true;
    return AuthResult::Ok;
}

AuthResult changePassword(AuthState& a, const std::string& loginName, const std::string& current,
                          const std::string& next, const std::string& keepToken) {
    User* u = ulFind(a.users, loginName);
    if (u == nullptr || current.size() > (size_t)MAX_PASSWORD_LEN) return AuthResult::BadCredentials;
    time_t now = a.clock->now();
    if (now < u->lockedUntil) return AuthResult::Locked;
    if (!verifyPassword(u->passwordHash, current)) {
        u->failedLogins++;
        if (u->failedLogins >= a.cfg.lockoutFailures) {
            u->failedLogins = 0;
            u->lockedUntil = now + (time_t)a.cfg.lockoutMinutes * 60;
            return AuthResult::Locked;
        }
        return AuthResult::BadCredentials;
    }
    u->failedLogins = 0;
    if (!validPassword(next) || next == current) return AuthResult::WeakPassword;
    std::string hash = hashPassword(next);
    if (hash.empty()) return AuthResult::CryptoError;
    u->passwordHash = hash;

    // Sign the account out everywhere else (linked-list removal).
    Session* prev = nullptr;
    Session* s = a.sessions.head;
    while (s != nullptr) {
        Session* following = s->next;
        if (s->login == loginName && s->token != keepToken) removeAfter(a.sessions, prev, s);
        else prev = s;
        s = following;
    }
    return AuthResult::Ok;
}
