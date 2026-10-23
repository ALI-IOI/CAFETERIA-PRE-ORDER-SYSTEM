#include "ds/action_stack.h"

void stInit(ActionStack& s) {
    s.top = -1;
}

bool stIsEmpty(const ActionStack& s) {
    return s.top == -1;
}

bool stIsFull(const ActionStack& s) {
    return s.top == ACTION_STACK_CAPACITY - 1;
}

int stSize(const ActionStack& s) {
    return s.top + 1;
}

bool stPush(ActionStack& s, const StaffAction& a) {
    if (stIsFull(s)) return false;
    s.top++;
    s.items[s.top] = a;
    return true;
}

bool stPop(ActionStack& s, StaffAction& out) {
    if (stIsEmpty(s)) return false;
    out = s.items[s.top];
    s.top--;
    return true;
}

const StaffAction* stPeek(const ActionStack& s) {
    if (stIsEmpty(s)) return nullptr;
    return &s.items[s.top];
}

const StaffAction* stFromTop(const ActionStack& s, int i) {
    if (i < 0 || i > s.top) return nullptr;
    return &s.items[s.top - i];
}
