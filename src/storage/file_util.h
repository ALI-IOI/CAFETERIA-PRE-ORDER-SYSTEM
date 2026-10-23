// Small helpers for the text-file storage.
#pragma once
#include <string>

// Writes `content` to `path` via a temporary file and a replace-rename, so a
// crash mid-write leaves the previous file intact.
bool atomicWrite(const std::string& path, const std::string& content, std::string& err);

// Appends `content` to `path` (creating it if needed).
bool appendFile(const std::string& path, const std::string& content, std::string& err);

bool fileExists(const std::string& path);
bool ensureDir(const std::string& path, std::string& err);

// Splits `line` at every `sep` into out[0..max-1]. Returns the number of fields,
// or -1 if there are more than `max`.
int splitFields(const std::string& line, char sep, std::string* out, int max);

// Strict base-10 integer: optional leading '-', digits only, no spaces, no
// overflow. Rejects "", "+1", "1e3", "12abc", " 5".
bool parseIntStrict(const std::string& s, long long& out);
bool parseIntStrict(const std::string& s, int& out);
