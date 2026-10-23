// Input checks applied by the server to every field (proposal section 10).
// Text that reaches storage may never contain '|' or control characters,
// because the data files are '|'-separated lines.
#pragma once
#include <string>

const int MAX_EMAIL_LEN = 100;
const int MAX_NAME_LEN = 60;
const int MIN_PASSWORD_LEN = 8;    // [CONFIRM]
const int MAX_PASSWORD_LEN = 128;  // [CONFIRM]
const int MIN_UNI_ID_LEN = 6;      // [CONFIRM] university ID format
const int MAX_UNI_ID_LEN = 20;

std::string toLower(const std::string& s);
bool validEmail(const std::string& s);
std::string emailDomain(const std::string& email);  // lower-case part after '@'
bool validUniversityId(const std::string& s);       // digits only
bool validName(const std::string& s);               // 1..60, no '|' or control chars
bool validPassword(const std::string& s);           // 8..128, no control chars
bool validStaffLogin(const std::string& s);         // 3..32 of [a-z0-9._-]
bool hasControlOrPipe(const std::string& s);
