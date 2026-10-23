#include "auth/validation.h"

std::string toLower(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    return out;
}

bool hasControlOrPipe(const std::string& s) {
    for (unsigned char c : s) {
        if (c < 0x20 || c == 0x7f || c == '|') return true;
    }
    return false;
}

static bool isAlnum(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool validEmail(const std::string& s) {
    if (s.size() < 3 || (int)s.size() > MAX_EMAIL_LEN) return false;
    size_t at = s.find('@');
    if (at == std::string::npos || at == 0 || s.find('@', at + 1) != std::string::npos) return false;
    std::string local = s.substr(0, at), domain = s.substr(at + 1);
    for (char c : local) {
        if (!isAlnum(c) && c != '.' && c != '_' && c != '-' && c != '+') return false;
    }
    if (domain.empty() || domain.front() == '.' || domain.back() == '.' ||
        domain.find('.') == std::string::npos || domain.find("..") != std::string::npos)
        return false;
    for (char c : domain) {
        if (!isAlnum(c) && c != '.' && c != '-') return false;
    }
    return true;
}

std::string emailDomain(const std::string& email) {
    size_t at = email.find('@');
    return at == std::string::npos ? "" : toLower(email.substr(at + 1));
}

bool validUniversityId(const std::string& s) {
    if ((int)s.size() < MIN_UNI_ID_LEN || (int)s.size() > MAX_UNI_ID_LEN) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

bool validName(const std::string& s) {
    return !s.empty() && (int)s.size() <= MAX_NAME_LEN && !hasControlOrPipe(s);
}

bool validPassword(const std::string& s) {
    if ((int)s.size() < MIN_PASSWORD_LEN || (int)s.size() > MAX_PASSWORD_LEN) return false;
    for (unsigned char c : s) {
        if (c < 0x20 || c == 0x7f) return false;
    }
    return true;
}

bool validStaffLogin(const std::string& s) {
    if (s.size() < 3 || s.size() > 32) return false;
    for (char c : s) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}
