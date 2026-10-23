#include "storage/file_util.h"
#include <climits>
#include <cstdio>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

bool atomicWrite(const std::string& path, const std::string& content, std::string& err) {
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            err = "cannot write " + tmp;
            return false;
        }
        out << content;
        out.flush();
        if (!out) {
            err = "write failed for " + tmp;
            return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        err = "cannot replace " + path;
        return false;
    }
#else
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        err = "cannot replace " + path;
        return false;
    }
#endif
    return true;
}

bool appendFile(const std::string& path, const std::string& content, std::string& err) {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) {
        err = "cannot append to " + path;
        return false;
    }
    out << content;
    return (bool)out;
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool ensureDir(const std::string& path, std::string& err) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        err = "cannot create directory " + path;
        return false;
    }
    return true;
}

int splitFields(const std::string& line, char sep, std::string* out, int max) {
    int n = 0;
    std::string cur;
    for (char ch : line) {
        if (ch == sep) {
            if (n >= max) return -1;
            out[n++] = cur;
            cur.clear();
        } else {
            cur += ch;
        }
    }
    if (n >= max) return -1;
    out[n++] = cur;
    return n;
}

bool parseIntStrict(const std::string& s, long long& out) {
    if (s.empty() || s.size() > 19) return false;
    size_t i = 0;
    bool neg = false;
    if (s[0] == '-') {
        neg = true;
        i = 1;
        if (s.size() == 1) return false;
    }
    long long v = 0;
    for (; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        int d = s[i] - '0';
        if (v > (LLONG_MAX - d) / 10) return false;
        v = v * 10 + d;
    }
    out = neg ? -v : v;
    return true;
}

bool parseIntStrict(const std::string& s, int& out) {
    long long v;
    if (!parseIntStrict(s, v) || v < INT_MIN || v > INT_MAX) return false;
    out = (int)v;
    return true;
}
