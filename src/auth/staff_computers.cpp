#include "auth/staff_computers.h"
#include "auth/validation.h"
#include <fstream>

void staffComputersInit(StaffComputers& sc) { sc.count = 0; }

// a.b.c.d, each part 0-255 without leading zeros.
static bool validIPv4(const std::string& s) {
    int parts = 0;
    size_t i = 0;
    while (i <= s.size()) {
        size_t j = i;
        while (j < s.size() && s[j] >= '0' && s[j] <= '9') j++;
        size_t len = j - i;
        if (len == 0 || len > 3 || (len > 1 && s[i] == '0')) return false;
        if (std::stoi(s.substr(i, len)) > 255) return false;
        parts++;
        if (j == s.size()) break;
        if (s[j] != '.') return false;
        i = j + 1;
    }
    return parts == 4;
}

// Hex digits and ':' with at least two ':', optionally ending in a dotted quad.
static bool validIPv6(const std::string& s) {
    if (s.size() < 2 || s.size() > 45) return false;
    size_t colons = 0, lastColon = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == ':') {
            colons++;
            lastColon = i;
        }
    }
    if (colons < 2) return false;
    std::string tail = s.substr(lastColon + 1);
    bool dottedTail = tail.find('.') != std::string::npos;
    if (dottedTail && !validIPv4(tail)) return false;
    size_t end = dottedTail ? lastColon : s.size();
    for (size_t i = 0; i < end; i++) {
        char c = s[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!hex && c != ':') return false;
    }
    return true;
}

static std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) b++;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) e--;
    return s.substr(b, e - b);
}

bool staffComputersLoad(StaffComputers& sc, const std::string& path, std::string& err) {
    staffComputersInit(sc);
    std::ifstream in(path);
    if (!in) return true;  // no file: empty list
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        std::string a = toLower(trim(line));
        if (a.empty() || a[0] == '#') continue;
        if (!validIPv4(a) && !validIPv6(a)) {
            err = path + " line " + std::to_string(lineNo) + ": not an IP address";
            return false;
        }
        if (sc.count == MAX_STAFF_COMPUTERS) {
            err = path + " line " + std::to_string(lineNo) + ": more than " + std::to_string(MAX_STAFF_COMPUTERS) +
                  " computers";
            return false;
        }
        sc.addr[sc.count++] = a;
    }
    return true;
}

bool staffComputerAllowed(const StaffComputers& sc, const std::string& remoteAddr) {
    if (sc.count == 0) return true;
    std::string a = toLower(remoteAddr);
    const std::string mapped = "::ffff:";
    if (a.rfind(mapped, 0) == 0 && validIPv4(a.substr(mapped.size()))) a = a.substr(mapped.size());
    for (int i = 0; i < sc.count; i++)
        if (sc.addr[i] == a) return true;
    return false;
}

bool isStaffPath(const std::string& path) {
    std::string p = toLower(path);
    size_t start = 0;
    for (size_t i = 0; i <= p.size(); i++) {
        if (i == p.size() || p[i] == '/' || p[i] == '\\') {
            if (p.compare(start, i - start, "staff") == 0 && i - start == 5) return true;
            start = i + 1;
        }
    }
    return false;
}
