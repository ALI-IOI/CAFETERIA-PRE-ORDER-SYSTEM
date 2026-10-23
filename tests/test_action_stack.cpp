#include "testing.h"
#include "ds/action_stack.h"

static StaffAction action(int n) {
    StaffAction a;
    a.type = ActionType::SetDailyLimit;
    a.staff = "staff1";
    a.itemIndex = 0;
    a.reservationId = 0;
    a.oldValue = n - 1;
    a.newValue = n;
    a.at = 0;
    a.description = "set limit to " + std::to_string(n);
    return a;
}

TEST(stack_pop_empty_returns_false) {
    ActionStack s;
    stInit(s);
    StaffAction out;
    CHECK(stIsEmpty(s));
    CHECK(!stPop(s, out));
    CHECK_EQ(stSize(s), 0);
}

TEST(stack_peek_empty_null) {
    ActionStack s;
    stInit(s);
    CHECK(stPeek(s) == nullptr);
}

TEST(stack_lifo_order) {
    ActionStack s;
    stInit(s);
    stPush(s, action(1));
    stPush(s, action(2));
    stPush(s, action(3));
    CHECK_EQ(stPeek(s)->newValue, 3);
    StaffAction out;
    CHECK(stPop(s, out));
    CHECK_EQ(out.newValue, 3);
    CHECK(stPop(s, out));
    CHECK_EQ(out.newValue, 2);
    CHECK(stPop(s, out));
    CHECK_EQ(out.newValue, 1);
    CHECK(stIsEmpty(s));
}

TEST(stack_push_full_returns_false_top_unchanged) {
    ActionStack s;
    stInit(s);
    for (int i = 1; i <= ACTION_STACK_CAPACITY; i++) CHECK(stPush(s, action(i)));
    CHECK(stIsFull(s));
    CHECK(!stPush(s, action(999)));
    CHECK_EQ(stSize(s), ACTION_STACK_CAPACITY);
    CHECK_EQ(stPeek(s)->newValue, ACTION_STACK_CAPACITY);
}

TEST(stack_from_top_index_for_history_display) {
    ActionStack s;
    stInit(s);
    stPush(s, action(1));
    stPush(s, action(2));
    CHECK_EQ(stFromTop(s, 0)->newValue, 2);
    CHECK_EQ(stFromTop(s, 1)->newValue, 1);
    CHECK(stFromTop(s, 2) == nullptr);
    CHECK(stFromTop(s, -1) == nullptr);
}
