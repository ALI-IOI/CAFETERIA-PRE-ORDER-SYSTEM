// Thin wrapper over libsodium (proposal section 10: no custom cryptography).
#pragma once
#include <string>

// Must succeed once before any other call. Safe to call repeatedly.
bool cryptoInit();

// Argon2id with a random salt, encoded as "$argon2id$v=19$m=...". Empty on failure.
std::string hashPassword(const std::string& password);
bool verifyPassword(const std::string& encodedHash, const std::string& password);

// `bytes` random bytes as lower-case hex (2*bytes characters).
std::string randomToken(int bytes);

// `n` random decimal digits, e.g. a 6-digit verification code.
std::string randomDigits(int n);

// Constant-time comparison of two strings of equal length.
bool constantTimeEquals(const std::string& a, const std::string& b);
