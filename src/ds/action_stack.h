// Staff action history: an array-based stack (proposal section 7).
// Push/pop/peek are O(1); the latest action is the first one undone (LIFO).
#pragma once
#include <ctime>
#include <string>

enum class ActionType { SetStock, SetDailyLimit, SetSetting, MarkCollected };

struct StaffAction {
    ActionType type;
    std::string staff;        // login of the staff member who did it
    int itemIndex;            // SetStock / SetDailyLimit
    int reservationId;        // MarkCollected
    std::string settingKey;   // SetSetting
    int oldValue;
    int newValue;
    time_t at;
    std::string description;  // e.g. "Set Tehari daily limit 10 -> 20"
};

const int ACTION_STACK_CAPACITY = 50;

struct ActionStack {
    StaffAction items[ACTION_STACK_CAPACITY];
    int top;  // index of the top element; -1 when empty
};

void stInit(ActionStack& s);
bool stIsEmpty(const ActionStack& s);
bool stIsFull(const ActionStack& s);
int stSize(const ActionStack& s);

// Returns false (and changes nothing) when the stack is full.
bool stPush(ActionStack& s, const StaffAction& a);

// Returns false when the stack is empty.
bool stPop(ActionStack& s, StaffAction& out);

// nullptr when empty.
const StaffAction* stPeek(const ActionStack& s);

// i-th element counted from the top (0 = top), for the history display.
const StaffAction* stFromTop(const ActionStack& s, int i);
