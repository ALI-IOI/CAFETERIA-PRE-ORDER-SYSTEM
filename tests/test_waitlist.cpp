#include "testing.h"
#include "cafe_fixture.h"

static const char* STAFF = "staff1";
static const char* DAVE = "dave@uni.example";

// Fried Rice with only `limit` portions, all taken by ALICE (1 each time).
static void sellOutFriedRice(CafeFixture& f, int limit) {
    f.caf.menu.items[FRIED_RICE].dailyLimit = limit;
    for (int i = 0; i < limit; i++) {
        std::string u = "buyer" + std::to_string(i) + "@uni.example";
        f.reserveOk(u.c_str(), FRIED_RICE, 1);
    }
}

static int countTo(const NotificationList& out, NotifType t, const char* to) {
    int n = 0;
    for (Notification* x = out.head; x; x = x->next)
        if (x->type == t && x->to == to) n++;
    return n;
}

TEST(join_only_when_sold_out) {
    CafeFixture f;
    int pos = 0;
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::NotSoldOut);
    sellOutFriedRice(f, 2);
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::Ok);
    CHECK_EQ(pos, 1);
    CHECK_EQ(waitlistPosition(f.caf, BOB, FRIED_RICE), 1);
    CHECK_EQ(joinWaitlist(f.caf, BOB, LAYS, 1, pos), Result::NotFresh);
    CHECK_EQ(joinWaitlist(f.caf, CAROL, FRIED_RICE, 0, pos), Result::BadQuantity);
}

TEST(join_twice_refused) {
    CafeFixture f;
    sellOutFriedRice(f, 1);
    int pos;
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::Ok);
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::AlreadyWaiting);
    CHECK_EQ(waitlistActiveCount(f.caf, FRIED_RICE), 1);
}

TEST(join_counts_portions_already_held) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 2;
    f.reserveOk(BOB, FRIED_RICE, 1);
    f.reserveOk(ALICE, FRIED_RICE, 1);  // sold out
    int pos;
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 2, pos), Result::UserLimit);  // 1 held + 2 > 2
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::Ok);
}

TEST(join_after_cutoff_refused) {
    CafeFixture f;
    sellOutFriedRice(f, 1);
    f.clock.set(2026, 10, 23, 10, 0, 0);
    int pos;
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::CutoffPassed);
}

TEST(waitlist_full_refused) {
    CafeFixture f(2);  // queue capacity 2
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    f.reserveOk(ALICE, FRIED_RICE, 1);
    int pos;
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::Ok);
    CHECK_EQ(joinWaitlist(f.caf, CAROL, FRIED_RICE, 1, pos), Result::Ok);
    CHECK_EQ(joinWaitlist(f.caf, DAVE, FRIED_RICE, 1, pos), Result::WaitlistFull);
}

TEST(cancel_promotes_first_waiting_with_email) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    Reservation* a = f.reserveOk(ALICE, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);
    joinWaitlist(f.caf, CAROL, FRIED_RICE, 1, pos);
    nlClear(f.out);
    CHECK_EQ(cancelReservation(f.caf, ALICE, a->id, f.out), Result::Ok);
    CHECK_EQ(userPortionsToday(f.caf, BOB, FRIED_RICE), 1);    // BOB got it
    CHECK_EQ(userPortionsToday(f.caf, CAROL, FRIED_RICE), 0);
    CHECK_EQ(menuRemaining(f.caf.menu.items[FRIED_RICE]), 0);
    CHECK_EQ(waitlistPosition(f.caf, CAROL, FRIED_RICE), 1);    // CAROL moved up
    CHECK_EQ(waitlistPosition(f.caf, BOB, FRIED_RICE), 0);
    CHECK_EQ(countTo(f.out, NotifType::Confirmation, BOB), 1);
    CHECK(f.out.tail->body.find("waitlist") != std::string::npos);
    CHECK_EQ(f.caf.cooking[FRIED_RICE].count, 2);               // ALICE (cancelled) + BOB
}

TEST(promotion_is_strict_fifo) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    Reservation* a = f.reserveOk(ALICE, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 2, pos);    // needs 2
    joinWaitlist(f.caf, CAROL, FRIED_RICE, 1, pos);  // needs 1
    cancelReservation(f.caf, ALICE, a->id, f.out);   // only 1 portion free
    CHECK_EQ(userPortionsToday(f.caf, CAROL, FRIED_RICE), 0);  // must not jump ahead of BOB
    CHECK_EQ(menuRemaining(f.caf.menu.items[FRIED_RICE]), 1);
    CHECK_EQ(waitlistPosition(f.caf, BOB, FRIED_RICE), 1);
}

TEST(left_entries_are_skipped) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    Reservation* a = f.reserveOk(ALICE, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);
    joinWaitlist(f.caf, CAROL, FRIED_RICE, 1, pos);
    CHECK_EQ(waitlistPosition(f.caf, CAROL, FRIED_RICE), 2);
    CHECK_EQ(leaveWaitlist(f.caf, BOB, FRIED_RICE), Result::Ok);
    CHECK_EQ(leaveWaitlist(f.caf, BOB, FRIED_RICE), Result::NotWaiting);
    CHECK_EQ(waitlistPosition(f.caf, CAROL, FRIED_RICE), 1);
    CHECK_EQ(waitlistActiveCount(f.caf, FRIED_RICE), 1);
    cancelReservation(f.caf, ALICE, a->id, f.out);
    CHECK_EQ(userPortionsToday(f.caf, BOB, FRIED_RICE), 0);
    CHECK_EQ(userPortionsToday(f.caf, CAROL, FRIED_RICE), 1);
}

TEST(promotion_skips_person_now_at_limit) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 3;
    f.reserveOk(ALICE, FRIED_RICE, 2);
    Reservation* c = f.reserveOk(CAROL, FRIED_RICE, 1);  // sold out
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 2, pos);
    joinWaitlist(f.caf, DAVE, FRIED_RICE, 1, pos);
    // BOB gets portions elsewhere: staff raise then lower would be odd, so give him 1 directly
    f.caf.menu.items[FRIED_RICE].dailyLimit = 4;
    CHECK(f.reserveOk(BOB, FRIED_RICE, 1) != nullptr);   // BOB now holds 1; waiting for 2 would exceed 2
    cancelReservation(f.caf, CAROL, c->id, f.out);        // 1 free
    CHECK_EQ(userPortionsToday(f.caf, BOB, FRIED_RICE), 1);  // not over the limit
    CHECK_EQ(userPortionsToday(f.caf, DAVE, FRIED_RICE), 1); // next in line served
}

TEST(limit_raise_promotes) {
    CafeFixture f;
    sellOutFriedRice(f, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);
    joinWaitlist(f.caf, CAROL, FRIED_RICE, 1, pos);
    bool rec;
    CHECK_EQ(setDailyLimit(f.caf, STAFF, FRIED_RICE, 2, f.out, rec), Result::Ok);
    CHECK_EQ(userPortionsToday(f.caf, BOB, FRIED_RICE), 1);
    CHECK_EQ(userPortionsToday(f.caf, CAROL, FRIED_RICE), 0);
}

TEST(undo_that_raises_limit_promotes) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 3;
    f.reserveOk(ALICE, FRIED_RICE, 1);
    bool rec;
    setDailyLimit(f.caf, STAFF, FRIED_RICE, 1, f.out, rec);  // 3 -> 1: sold out
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);
    StaffAction u;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, u), Result::Ok);  // back to 3
    CHECK_EQ(userPortionsToday(f.caf, BOB, FRIED_RICE), 1);
}

TEST(no_promotion_after_cutoff) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    f.reserveOk(ALICE, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);
    f.clock.set(2026, 10, 23, 10, 30, 0);
    bool rec;
    setDailyLimit(f.caf, STAFF, FRIED_RICE, 2, f.out, rec);
    CHECK_EQ(userPortionsToday(f.caf, BOB, FRIED_RICE), 0);
}

TEST(cutoff_closes_waitlist_with_one_email_each) {
    CafeFixture f;
    sellOutFriedRice(f, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);
    joinWaitlist(f.caf, CAROL, FRIED_RICE, 1, pos);
    leaveWaitlist(f.caf, CAROL, FRIED_RICE);
    nlClear(f.out);
    f.clock.set(2026, 10, 23, 9, 59, 0);
    timerTick(f.caf, f.out);
    CHECK_EQ(countTo(f.out, NotifType::WaitlistClosed, BOB), 0);  // not yet
    f.clock.set(2026, 10, 23, 10, 0, 0);
    timerTick(f.caf, f.out);
    timerTick(f.caf, f.out);
    CHECK_EQ(countTo(f.out, NotifType::WaitlistClosed, BOB), 1);   // exactly once
    CHECK_EQ(countTo(f.out, NotifType::WaitlistClosed, CAROL), 0); // she had left
    CHECK_EQ(waitlistActiveCount(f.caf, FRIED_RICE), 0);
}

TEST(rollover_clears_waitlist) {
    CafeFixture f;
    sellOutFriedRice(f, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);
    f.clock.set(2026, 10, 24, 8, 0, 0);
    cafEnsureToday(f.caf);  // no tick ran past yesterday's cutoff
    CHECK_EQ(waitlistActiveCount(f.caf, FRIED_RICE), 0);
    CHECK_EQ(f.caf.waitlists[FRIED_RICE].count, 0);
}

TEST(cancel_and_promote_use_portion_once) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    Reservation* a = f.reserveOk(ALICE, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);
    cancelReservation(f.caf, ALICE, a->id, f.out);
    // the freed portion went to BOB; nobody else can take it
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, CAROL, FRIED_RICE, 1, f.out, &r), Result::SoldOut);
    CHECK_EQ(f.caf.menu.items[FRIED_RICE].reservedToday, 1);
}

// ---- final review fixes ------------------------------------------------------

TEST(direct_reserve_leaves_the_waitlist) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 2;
    Reservation* a = f.reserveOk(ALICE, FRIED_RICE, 1);
    Reservation* b = f.reserveOk(BOB, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, CAROL, FRIED_RICE, 2, pos);
    joinWaitlist(f.caf, DAVE, FRIED_RICE, 1, pos);
    cancelReservation(f.caf, ALICE, a->id, f.out);        // 1 free, CAROL needs 2: nobody promoted
    CHECK(f.reserveOk(DAVE, FRIED_RICE, 1) != nullptr);  // DAVE takes it himself
    CHECK_EQ(waitlistPosition(f.caf, DAVE, FRIED_RICE), 0);  // ...and is off the waitlist
    leaveWaitlist(f.caf, CAROL, FRIED_RICE);
    cancelReservation(f.caf, BOB, b->id, f.out);
    CHECK_EQ(userPortionsToday(f.caf, DAVE, FRIED_RICE), 1);  // never booked a second time
}

TEST(mark_ready_promotes_after_queue_full_stall) {
    CafeFixture f(3);  // cooking queue capacity 3
    f.caf.menu.items[FRIED_RICE].dailyLimit = 3;
    Reservation* a = f.reserveOk(ALICE, FRIED_RICE, 1);
    f.reserveOk(BOB, FRIED_RICE, 1);
    f.reserveOk(CAROL, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, DAVE, FRIED_RICE, 1, pos);
    cancelReservation(f.caf, ALICE, a->id, f.out);  // portion free, but the queue still holds 3 entries
    CHECK_EQ(waitlistPosition(f.caf, DAVE, FRIED_RICE), 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, FRIED_RICE, 0, f.out, rep);  // empties the cooking queue
    CHECK_EQ(userPortionsToday(f.caf, DAVE, FRIED_RICE), 1);
    CHECK_EQ(waitlistPosition(f.caf, DAVE, FRIED_RICE), 0);
}

TEST(cancel_does_not_hand_the_portion_back_to_the_canceller) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    Reservation* b = f.reserveOk(BOB, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);    // BOB wants one more, first in line
    joinWaitlist(f.caf, CAROL, FRIED_RICE, 1, pos);
    cancelReservation(f.caf, BOB, b->id, f.out);
    CHECK_EQ(userPortionsToday(f.caf, BOB, FRIED_RICE), 0);   // cancelling means fewer, not a new booking
    CHECK_EQ(waitlistPosition(f.caf, BOB, FRIED_RICE), 0);
    CHECK_EQ(userPortionsToday(f.caf, CAROL, FRIED_RICE), 1); // next person gets it
}

TEST(full_waitlist_compacts_entries_that_left) {
    CafeFixture f(3);
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    f.reserveOk(ALICE, FRIED_RICE, 1);
    int pos;
    for (int i = 0; i < 3; i++) {
        CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::Ok);
        CHECK_EQ(leaveWaitlist(f.caf, BOB, FRIED_RICE), Result::Ok);
    }
    CHECK_EQ(joinWaitlist(f.caf, CAROL, FRIED_RICE, 1, pos), Result::Ok);  // nobody is really waiting
    CHECK_EQ(pos, 1);
}
