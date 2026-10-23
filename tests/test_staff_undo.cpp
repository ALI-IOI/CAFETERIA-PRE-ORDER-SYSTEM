#include "testing.h"
#include "cafe_fixture.h"

static const char* STAFF = "staff1";

TEST(set_stock_then_undo_restores) {
    CafeFixture f;
    bool rec = false;
    CHECK_EQ(setStock(f.caf, STAFF, LAYS, 35, rec), Result::Ok);
    CHECK(rec);
    CHECK_EQ(f.caf.menu.items[LAYS].stock, 35);
    CHECK_EQ(stPeek(f.caf.history)->description, std::string("Set Lays stock 20 -> 35"));
    StaffAction undone;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, undone), Result::Ok);
    CHECK_EQ(f.caf.menu.items[LAYS].stock, 20);
    CHECK_EQ(undone.type, ActionType::SetStock);
    CHECK(stIsEmpty(f.caf.history));
}

TEST(set_stock_rejects_bad_values_and_fresh_items) {
    CafeFixture f;
    bool rec = true;
    CHECK_EQ(setStock(f.caf, STAFF, LAYS, -1, rec), Result::InvalidValue);
    CHECK(!rec);
    CHECK_EQ(setStock(f.caf, STAFF, LAYS, 10000, rec), Result::InvalidValue);
    CHECK_EQ(setStock(f.caf, STAFF, TEHARI, 5, rec), Result::WrongKind);
    CHECK_EQ(setDailyLimit(f.caf, STAFF, LAYS, 5, f.out, rec), Result::WrongKind);
    CHECK_EQ(setStock(f.caf, STAFF, 50, 5, rec), Result::BadItem);
    CHECK(stIsEmpty(f.caf.history));
}

TEST(undo_empty_is_NothingToUndo) {
    CafeFixture f;
    StaffAction undone;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, undone), Result::NothingToUndo);
}

TEST(undo_is_lifo_across_three_actions) {
    CafeFixture f;
    bool rec;
    setDailyLimit(f.caf, STAFF, TEHARI, 20, f.out, rec);
    setStock(f.caf, STAFF, LAYS, 5, rec);
    changeSetting(f.caf, STAFF, "cutoff", 11 * 60, rec);
    CHECK_EQ(stPeek(f.caf.history)->description, std::string("Changed cutoff 10:00 -> 11:00"));
    StaffAction u;
    undoLast(f.caf, STAFF, f.out, u);
    CHECK_EQ(u.type, ActionType::SetSetting);
    CHECK_EQ(f.caf.settings.cutoffMinute, 600);
    undoLast(f.caf, STAFF, f.out, u);
    CHECK_EQ(u.type, ActionType::SetStock);
    CHECK_EQ(f.caf.menu.items[LAYS].stock, 20);
    undoLast(f.caf, STAFF, f.out, u);
    CHECK_EQ(u.type, ActionType::SetDailyLimit);
    CHECK_EQ(f.caf.menu.items[TEHARI].dailyLimit, 10);
}

TEST(limit_below_reserved_refused) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 2);
    f.reserveOk(BOB, TEHARI, 2);
    bool rec;
    CHECK_EQ(setDailyLimit(f.caf, STAFF, TEHARI, 3, f.out, rec), Result::InvalidValue);
    CHECK_EQ(f.caf.menu.items[TEHARI].dailyLimit, 10);
    CHECK_EQ(setDailyLimit(f.caf, STAFF, TEHARI, 4, f.out, rec), Result::Ok);
    CHECK_EQ(setDailyLimit(f.caf, STAFF, TEHARI, 101, f.out, rec), Result::InvalidValue);  // over queue capacity
}

TEST(undo_blocked_when_limit_now_below_reserved) {
    CafeFixture f;
    bool rec;
    setDailyLimit(f.caf, STAFF, TEHARI, 2, f.out, rec);   // 10 -> 2
    setDailyLimit(f.caf, STAFF, TEHARI, 12, f.out, rec);  // 2 -> 12
    for (int i = 0; i < 5; i++) {
        std::string u = "user" + std::to_string(i) + "@uni.example";
        CHECK(f.reserveOk(u.c_str(), TEHARI, 1) != nullptr);
    }
    StaffAction undone;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, undone), Result::UndoBlocked);  // would set 2 < 5 reserved
    CHECK_EQ(f.caf.menu.items[TEHARI].dailyLimit, 12);
    CHECK_EQ(stSize(f.caf.history), 2);
    CHECK_EQ(stPeek(f.caf.history)->newValue, 12);  // pushed back unchanged
}

TEST(stack_full_action_applies_but_not_recorded) {
    CafeFixture f;
    bool rec = false;
    for (int i = 1; i <= ACTION_STACK_CAPACITY; i++) {
        CHECK_EQ(setStock(f.caf, STAFF, LAYS, i, rec), Result::Ok);
        CHECK(rec);
    }
    CHECK_EQ(setStock(f.caf, STAFF, LAYS, 777, rec), Result::Ok);
    CHECK(!rec);
    CHECK_EQ(f.caf.menu.items[LAYS].stock, 777);
    CHECK_EQ(stSize(f.caf.history), ACTION_STACK_CAPACITY);
}

TEST(undo_collected_restores_ready_and_reminders) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    bool rec;
    markCollected(f.caf, STAFF, a->id, rec);
    // let the collected entry leave the reminder queue
    f.clock.advanceMinutes(61);
    timerTick(f.caf, f.out);
    CHECK(qIsEmpty(f.caf.reminders));
    StaffAction u;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, u), Result::Ok);
    CHECK_EQ(a->status, ResStatus::Ready);
    CHECK_EQ(f.caf.reminders.count, 1);
}

TEST(undo_collected_does_not_duplicate_queue_entry) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    bool rec;
    markCollected(f.caf, STAFF, a->id, rec);  // still physically in the reminder queue
    StaffAction u;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, u), Result::Ok);
    CHECK_EQ(f.caf.reminders.count, 1);
}

TEST(mark_ready_not_on_stack) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    CHECK(stIsEmpty(f.caf.history));
}

TEST(change_setting_invalid_refused) {
    CafeFixture f;
    bool rec;
    CHECK_EQ(changeSetting(f.caf, STAFF, "freshness_minutes", 10, rec), Result::InvalidValue);
    CHECK_EQ(changeSetting(f.caf, STAFF, "bogus", 10, rec), Result::InvalidValue);
    CHECK_EQ(changeSetting(f.caf, STAFF, "freshness_minutes", 480, rec), Result::Ok);
    CHECK_EQ(stPeek(f.caf.history)->description, std::string("Changed freshness_minutes 420 -> 480"));
}

TEST(sorted_by_item_then_time) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);      // 09:00
    f.clock.advanceMinutes(5);
    f.reserveOk(BOB, KHICHURI, 1);      // 09:05
    f.clock.advanceMinutes(5);
    f.reserveOk(CAROL, TEHARI, 1);      // 09:10
    f.clock.advanceMinutes(5);
    f.reserveOk(ALICE, KHICHURI, 1);    // 09:15
    Reservation* arr[10];
    int n = staffReservationsSorted(f.caf, arr, 10);
    CHECK_EQ(n, 4);
    CHECK_EQ(arr[0]->itemIndex, KHICHURI);
    CHECK_EQ(arr[0]->user, std::string(BOB));
    CHECK_EQ(arr[1]->itemIndex, KHICHURI);
    CHECK_EQ(arr[1]->user, std::string(ALICE));
    CHECK_EQ(arr[2]->user, std::string(ALICE));
    CHECK_EQ(arr[2]->itemIndex, TEHARI);
    CHECK_EQ(arr[3]->user, std::string(CAROL));
}

TEST(search_by_id_hit_and_miss) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    CHECK(staffSearchById(f.caf, a->id) == a);
    CHECK(staffSearchById(f.caf, 4242) == nullptr);
}
