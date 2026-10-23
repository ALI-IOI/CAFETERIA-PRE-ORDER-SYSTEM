#include "auth/crypto.h"
#include <sodium.h>

bool cryptoInit() {
    return sodium_init() >= 0;  // 0 = initialised now, 1 = already initialised
}

std::string hashPassword(const std::string& password) {
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(out, password.c_str(), password.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
        return "";
    return out;
}

bool verifyPassword(const std::string& encodedHash, const std::string& password) {
    if (encodedHash.empty() || encodedHash.size() >= crypto_pwhash_STRBYTES) return false;
    return crypto_pwhash_str_verify(encodedHash.c_str(), password.c_str(), password.size()) == 0;
}

std::string randomToken(int bytes) {
    unsigned char buf[64];
    if (bytes < 1 || bytes > 64) return "";
    randombytes_buf(buf, (size_t)bytes);
    char hex[129];
    sodium_bin2hex(hex, sizeof hex, buf, (size_t)bytes);
    return hex;
}

std::string randomDigits(int n) {
    std::string s;
    for (int i = 0; i < n; i++) s += (char)('0' + randombytes_uniform(10));
    return s;
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return sodium_memcmp(a.data(), b.data(), a.size()) == 0;
}
