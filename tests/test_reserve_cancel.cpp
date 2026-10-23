#include "testing.h"
#include "cafe_fixture.h"

TEST(reserve_before_cutoff_ok) {
    CafeFixture f;
    f.clock.set(2026, 10, 23, 9, 59, 59);
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, ALICE, TEHARI, 1, f.out, &r), Result::Ok);
    CHECK(r != nullptr);
    CHECK_EQ(r->status, ResStatus::Reserved);
    CHECK_EQ(r->day, 20261023);
    CHECK_EQ(f.caf.menu.items[TEHARI].reservedToday, 1);
    CHECK_EQ(f.caf.cooking[TEHARI].count, 1);
    CHECK(qPeek(f.caf.cooking[TEHARI]) == r);
}

TEST(reserve_at_cutoff_refused) {
    CafeFixture f;
    f.clock.set(2026, 10, 23, 10, 0, 0);
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, ALICE, TEHARI, 1, f.out, &r), Result::CutoffPassed);
    CHECK(r == nullptr);
    CHECK_EQ(f.caf.menu.items[TEHARI].reservedToday, 0);
    CHECK_EQ(f.caf.reservations.size, 0);
}

TEST(packaged_item_not_reservable) {
    CafeFixture f;
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, ALICE, LAYS, 1, f.out, &r), Result::NotFresh);
    CHECK_EQ(reserve(f.caf, ALICE, 99, 1, f.out, &r), Result::BadItem);
    CHECK_EQ(reserve(f.caf, ALICE, -1, 1, f.out, &r), Result::BadItem);
}

TEST(quantity_zero_negative_and_over_limit_refused) {
    CafeFixture f;
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, ALICE, TEHARI, 0, f.out, &r), Result::BadQuantity);
    CHECK_EQ(reserve(f.caf, ALICE, TEHARI, -3, f.out, &r), Result::BadQuantity);
    CHECK_EQ(reserve(f.caf, ALICE, TEHARI, 3, f.out, &r), Result::BadQuantity);  // per-user limit is 2
    CHECK_EQ(f.caf.reservations.size, 0);
}

TEST(sold_out_refused_and_partial_refused) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 3;
    CHECK(f.reserveOk(ALICE, FRIED_RICE, 2) != nullptr);
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, BOB, FRIED_RICE, 2, f.out, &r), Result::SoldOut);  // only 1 left
    CHECK(f.reserveOk(BOB, FRIED_RICE, 1) != nullptr);
    CHECK_EQ(reserve(f.caf, CAROL, FRIED_RICE, 1, f.out, &r), Result::SoldOut);
    CHECK_EQ(menuRemaining(f.caf.menu.items[FRIED_RICE]), 0);
}

TEST(per_user_limit_across_two_reservations) {
    CafeFixture f;
    Reservation* first = f.reserveOk(ALICE, KHICHURI, 1);
    CHECK(first != nullptr);
    CHECK(f.reserveOk(ALICE, KHICHURI, 1) != nullptr);
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, ALICE, KHICHURI, 1, f.out, &r), Result::UserLimit);
    CHECK_EQ(userPortionsToday(f.caf, ALICE, KHICHURI), 2);
    // a different item has its own limit
    CHECK(f.reserveOk(ALICE, TEHARI, 2) != nullptr);
    // cancelling frees one portion of the user's allowance
    CHECK_EQ(cancelReservation(f.caf, ALICE, first->id), Result::Ok);
    CHECK_EQ(userPortionsToday(f.caf, ALICE, KHICHURI), 1);
    CHECK(f.reserveOk(ALICE, KHICHURI, 1) != nullptr);
}

TEST(queue_full_rejects_reservation) {
    CafeFixture f(2);  // cooking queue capacity 2
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    CHECK(f.reserveOk(BOB, TEHARI, 1) != nullptr);
    // cancelled entries stay in the queue, so the queue is still full
    CHECK_EQ(cancelReservation(f.caf, ALICE, a->id), Result::Ok);
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, CAROL, TEHARI, 1, f.out, &r), Result::QueueFull);
    CHECK_EQ(f.caf.menu.items[TEHARI].reservedToday, 1);  // nothing leaked
    CHECK_EQ(f.caf.reservations.size, 2);
}

TEST(confirmation_notification_text) {
    CafeFixture f;
    Reservation* r = f.reserveOk(ALICE, TEHARI, 1);
    CHECK(r != nullptr);
    CHECK_EQ(f.out.count, 1);
    CHECK_EQ(f.out.head->type, NotifType::Confirmation);
    CHECK_EQ(f.out.head->to, std::string(ALICE));
    CHECK_EQ(f.out.head->reservationId, r->id);
    CHECK(f.out.head->subject.find("R-0001") != std::string::npos);
    CHECK(f.out.head->body.find("Reservation R-0001 confirmed: Tehari x 1") != std::string::npos);
}

TEST(cancel_before_deadline_returns_portion) {
    CafeFixture f;
    Reservation* r = f.reserveOk(ALICE, TEHARI, 2);
    CHECK_EQ(menuRemaining(f.caf.menu.items[TEHARI]), 8);
    f.clock.set(2026, 10, 23, 9, 59, 59);
    CHECK_EQ(cancelReservation(f.caf, ALICE, r->id), Result::Ok);
    CHECK_EQ(r->status, ResStatus::Cancelled);
    CHECK_EQ(menuRemaining(f.caf.menu.items[TEHARI]), 10);
}

TEST(cancel_after_deadline_keeps_portion) {
    CafeFixture f;
    Reservation* r = f.reserveOk(ALICE, TEHARI, 2);
    f.clock.set(2026, 10, 23, 10, 0, 0);
    CHECK_EQ(cancelReservation(f.caf, ALICE, r->id), Result::DeadlinePassed);
    CHECK_EQ(r->status, ResStatus::Reserved);
    CHECK_EQ(menuRemaining(f.caf.menu.items[TEHARI]), 8);
}

TEST(cancel_twice_refused) {
    CafeFixture f;
    Reservation* r = f.reserveOk(ALICE, TEHARI, 1);
    CHECK_EQ(cancelReservation(f.caf, ALICE, r->id), Result::Ok);
    CHECK_EQ(cancelReservation(f.caf, ALICE, r->id), Result::NotCancellable);
    CHECK_EQ(menuRemaining(f.caf.menu.items[TEHARI]), 10);  // returned only once
}

TEST(cancel_others_reservation_is_NotOwner) {
    CafeFixture f;
    Reservation* r = f.reserveOk(ALICE, TEHARI, 1);
    CHECK_EQ(cancelReservation(f.caf, BOB, r->id), Result::NotOwner);
    CHECK_EQ(cancelReservation(f.caf, BOB, 999), Result::NotFound);
    CHECK_EQ(r->status, ResStatus::Reserved);
}

TEST(cancel_ready_reservation_refused) {
    CafeFixture f;
    Reservation* r = f.reserveOk(ALICE, TEHARI, 1);
    r->status = ResStatus::Ready;  // markReady is Task 6; set directly here
    CHECK_EQ(cancelReservation(f.caf, ALICE, r->id), Result::NotCancellable);
}

TEST(settings_change_moves_cutoff) {
    CafeFixture f;
    CHECK(settingsSet(f.caf.settings, "cutoff", 11 * 60));
    f.clock.set(2026, 10, 23, 10, 30, 0);
    CHECK(f.reserveOk(ALICE, TEHARI, 1) != nullptr);
    CHECK(!settingsSet(f.caf.settings, "cutoff", 24 * 60));       // out of range
    CHECK(!settingsSet(f.caf.settings, "per_user_limit", 0));
    CHECK(!settingsSet(f.caf.settings, "no_such_key", 5));
    CHECK_EQ(f.caf.settings.cutoffMinute, 11 * 60);
}

TEST(list_user_reservations_only_returns_own) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    f.reserveOk(BOB, TEHARI, 1);
    f.reserveOk(ALICE, KHICHURI, 1);
    Reservation* arr[10];
    int n = listUserReservations(f.caf, ALICE, arr, 10);
    CHECK_EQ(n, 2);
    CHECK_EQ(arr[0]->user, std::string(ALICE));
    CHECK_EQ(arr[1]->user, std::string(ALICE));
}
