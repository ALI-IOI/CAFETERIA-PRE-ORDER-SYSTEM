#include "core/clock.h"
#include <cstdio>

time_t SystemClock::now() {
    return std::time(nullptr);
}

void FakeClock::set(int year, int month, int day, int hour, int minute, int second) {
    std::tm tmv = {};
    tmv.tm_year = year - 1900;
    tmv.tm_mon = month - 1;
    tmv.tm_mday = day;
    tmv.tm_hour = hour;
    tmv.tm_min = minute;
    tmv.tm_sec = second;
    tmv.tm_isdst = -1;
    t = std::mktime(&tmv);
}

static std::tm localOf(time_t t) {
    std::tm out = {};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

int dayKeyOf(time_t t) {
    std::tm tmv = localOf(t);
    return (tmv.tm_year + 1900) * 10000 + (tmv.tm_mon + 1) * 100 + tmv.tm_mday;
}

int secondOfDay(time_t t) {
    std::tm tmv = localOf(t);
    return tmv.tm_hour * 3600 + tmv.tm_min * 60 + tmv.tm_sec;
}

std::string formatClock(time_t t) {
    std::tm tmv = localOf(t);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    return buf;
}

std::string formatMinuteOfDay(int minuteOfDay) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", minuteOfDay / 60, minuteOfDay % 60);
    return buf;
}
