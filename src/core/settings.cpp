#include "core/settings.h"

const char* const SETTING_KEYS[] = {"cutoff", "cancel_deadline", "freshness_minutes",
                                    "reminder_minutes", "per_user_limit", nullptr};

Settings settingsDefaults() {
    Settings s;
    s.cutoffMinute = 10 * 60;
    s.cancelDeadlineMinute = 10 * 60;
    s.freshnessMinutes = 7 * 60;
    s.reminderIntervalMinutes = 60;
    s.perUserLimit = 2;
    return s;
}

// Returns a pointer to the field for `key` and its allowed range, or nullptr.
static int* field(Settings& s, const std::string& key, int& lo, int& hi) {
    if (key == "cutoff") { lo = 0; hi = 1439; return &s.cutoffMinute; }
    if (key == "cancel_deadline") { lo = 0; hi = 1439; return &s.cancelDeadlineMinute; }
    if (key == "freshness_minutes") { lo = 30; hi = 1440; return &s.freshnessMinutes; }
    if (key == "reminder_minutes") { lo = 5; hi = 720; return &s.reminderIntervalMinutes; }
    if (key == "per_user_limit") { lo = 1; hi = 10; return &s.perUserLimit; }
    return nullptr;
}

bool settingsSet(Settings& s, const std::string& key, int value) {
    int lo, hi;
    int* f = field(s, key, lo, hi);
    if (f == nullptr || value < lo || value > hi) return false;
    *f = value;
    return true;
}

bool settingsGet(const Settings& s, const std::string& key, int& value) {
    int lo, hi;
    Settings copy = s;
    int* f = field(copy, key, lo, hi);
    if (f == nullptr) return false;
    value = *f;
    return true;
}
