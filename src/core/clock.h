// All rule code reads the time through a Clock so tests can control it.
#pragma once
#include <ctime>
#include <string>

struct Clock {
    virtual time_t now() = 0;
    virtual ~Clock() {}
};

struct SystemClock : Clock {
    time_t now() override;
};

// Test / demo clock: time only moves when told to.
struct FakeClock : Clock {
    time_t t = 0;
    time_t now() override { return t; }
    void set(int year, int month, int day, int hour, int minute, int second);
    void advanceMinutes(int minutes) { t += (time_t)minutes * 60; }
};

// Local-time helpers.
int dayKeyOf(time_t t);         // 20261023
int secondOfDay(time_t t);      // 0 .. 86399
std::string formatClock(time_t t);    // "14:05"
std::string formatMinuteOfDay(int minuteOfDay);  // 600 -> "10:00"
