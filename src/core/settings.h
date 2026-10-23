// Staff-changeable rules (proposal section 4). These are data, not constants:
// the defaults below are the proposal's placeholders and are loaded from
// data/settings.txt when that file exists.
#pragma once
#include <string>

struct Settings {
    int cutoffMinute;             // minute of day; 600 = 10:00 [CONFIRM]
    int cancelDeadlineMinute;     // minute of day; 600 = 10:00 [CONFIRM]
    int freshnessMinutes;         // 420 = 7 hours after "ready" [CONFIRM]
    int reminderIntervalMinutes;  // 60 = hourly [CONFIRM]
    int perUserLimit;             // portions per user per item per day; 2 [CONFIRM]
};

Settings settingsDefaults();

// Keys: cutoff, cancel_deadline, freshness_minutes, reminder_minutes, per_user_limit.
// Returns false for an unknown key or a value outside its allowed range.
bool settingsSet(Settings& s, const std::string& key, int value);
bool settingsGet(const Settings& s, const std::string& key, int& value);

// Key names in display order, terminated by nullptr.
extern const char* const SETTING_KEYS[];
