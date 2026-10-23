#include "testing.h"
#include "cafe_fixture.h"

static const char* STAFF = "staff1";

static void priced(CafeFixture& f) {
    bool rec;
    setPrice(f.caf, STAFF, KHICHURI, 80, rec);
    setPrice(f.caf, STAFF, TEHARI, 120, rec);
    setPrice(f.caf, STAFF, FRIED_RICE, 100, rec);
}

TEST(batch_all_ok_one_email_with_total) {
    CafeFixture f;
    priced(f);
    TrayLine lines[] = {{TEHARI, 2}, {KHICHURI, 1}};
    int failed = -1, ids[2] = {0, 0};
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 2, f.out, failed, ids), Result::Ok);
    CHECK_EQ(failed, -1);
    CHECK_EQ(ids[0], 1);
    CHECK_EQ(ids[1], 2);
    CHECK_EQ(userPortionsToday(f.caf, ALICE, TEHARI), 2);
    CHECK_EQ(userPortionsToday(f.caf, ALICE, KHICHURI), 1);
    CHECK_EQ(f.out.count, 1);  // one email for the whole tray
    CHECK_EQ(f.out.head->subject, std::string("Reservations confirmed: R-0001, R-0002"));
    CHECK(f.out.head->body.find("R-0001  Tehari x 2  \xE0\xA7\xB3 240") != std::string::npos);
    CHECK(f.out.head->body.find("Total to pay at the counter: \xE0\xA7\xB3 320") != std::string::npos);
}

TEST(batch_counts_earlier_lines_of_same_dish) {
    CafeFixture f;
    TrayLine lines[] = {{TEHARI, 1}, {TEHARI, 2}};
    int failed = -1, ids[2];
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 2, f.out, failed, ids), Result::UserLimit);
    CHECK_EQ(failed, 1);
    CHECK_EQ(f.caf.reservations.size, 0);
    CHECK_EQ(f.caf.menu.items[TEHARI].reservedToday, 0);
}

TEST(batch_sold_out_line_reserves_nothing) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    TrayLine lines[] = {{KHICHURI, 1}, {TEHARI, 1}, {FRIED_RICE, 2}};
    int failed = -1, ids[3];
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 3, f.out, failed, ids), Result::SoldOut);
    CHECK_EQ(failed, 2);
    CHECK_EQ(f.caf.reservations.size, 0);
    CHECK_EQ(f.caf.menu.items[KHICHURI].reservedToday, 0);
    CHECK_EQ(f.caf.menu.items[TEHARI].reservedToday, 0);
    CHECK_EQ(f.caf.cooking[KHICHURI].count, 0);
    CHECK_EQ(f.out.count, 0);
}

TEST(batch_counts_remaining_across_lines) {
    CafeFixture f;
    f.caf.menu.items[TEHARI].dailyLimit = 2;
    f.caf.settings.perUserLimit = 3;
    TrayLine lines[] = {{TEHARI, 2}, {TEHARI, 1}};
    int failed = -1, ids[2];
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 2, f.out, failed, ids), Result::SoldOut);
    CHECK_EQ(failed, 1);
}

TEST(batch_refuses_unavailable_dish) {
    CafeFixture f;
    bool rec;
    setAvailable(f.caf, STAFF, TEHARI, false, f.out, rec);
    TrayLine lines[] = {{KHICHURI, 1}, {TEHARI, 1}};
    int failed = -1, ids[2];
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 2, f.out, failed, ids), Result::NotAvailable);
    CHECK_EQ(failed, 1);
    CHECK_EQ(f.caf.reservations.size, 0);
}

TEST(batch_after_cutoff_refused) {
    CafeFixture f;
    f.clock.set(2026, 10, 23, 10, 0, 0);
    TrayLine lines[] = {{KHICHURI, 1}};
    int failed = -1, ids[1];
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 1, f.out, failed, ids), Result::CutoffPassed);
    CHECK_EQ(failed, 0);
}

TEST(batch_queue_full_reserves_nothing) {
    CafeFixture f(1);  // cooking queues hold 1 entry
    TrayLine lines[] = {{KHICHURI, 1}, {KHICHURI, 1}};
    int failed = -1, ids[2];
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 2, f.out, failed, ids), Result::QueueFull);
    CHECK_EQ(failed, 1);
    CHECK_EQ(f.caf.reservations.size, 0);
}

TEST(batch_bad_input_refused) {
    CafeFixture f;
    TrayLine bad[] = {{LAYS, 1}};
    int failed = -1, ids[1];
    CHECK_EQ(reserveBatch(f.caf, ALICE, bad, 1, f.out, failed, ids), Result::NotFresh);
    CHECK_EQ(reserveBatch(f.caf, ALICE, bad, 0, f.out, failed, ids), Result::BadQuantity);
    TrayLine zero[] = {{TEHARI, 0}};
    CHECK_EQ(reserveBatch(f.caf, ALICE, zero, 1, f.out, failed, ids), Result::BadQuantity);
}

TEST(batch_removes_user_from_each_dishs_waitlist) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    Reservation* b = f.reserveOk(BOB, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, ALICE, FRIED_RICE, 1, pos);
    bool rec;
    setDailyLimit(f.caf, STAFF, FRIED_RICE, 1, f.out, rec);  // no change
    cancelReservation(f.caf, BOB, b->id, f.out);              // ALICE promoted
    nlClear(f.out);
    joinWaitlist(f.caf, CAROL, FRIED_RICE, 1, pos);
    f.caf.menu.items[TEHARI].dailyLimit = 10;
    TrayLine lines[] = {{TEHARI, 1}};
    int failed, ids[1];
    // CAROL is waiting for Fried Rice; reserving Tehari must not touch that
    CHECK_EQ(reserveBatch(f.caf, CAROL, lines, 1, f.out, failed, ids), Result::Ok);
    CHECK_EQ(waitlistPosition(f.caf, CAROL, FRIED_RICE), 1);
}

// A clock that jumps past the 10:00 cutoff after a given number of reads, so
// the cutoff can pass between checking a tray and booking it.
struct JumpClock : Clock {
    int readsLeft = 0;
    time_t before = 0, after = 0;
    time_t now() override { return readsLeft-- > 0 ? before : after; }
};

TEST(batch_cutoff_passing_mid_booking_is_all_or_nothing) {
    for (int k = 0; k < 40; k++) {
        CafeFixture f;
        JumpClock jc;
        jc.before = f.clock.t;           // 09:00
        jc.after = f.clock.t + 61 * 60;  // 10:01, past the cutoff
        jc.readsLeft = k;
        f.caf.clock = &jc;
        TrayLine lines[] = {{TEHARI, 1}, {KHICHURI, 1}, {FRIED_RICE, 1}};
        int failed = -1, ids[3] = {0, 0, 0};
        Result r = reserveBatch(f.caf, ALICE, lines, 3, f.out, failed, ids);
        if (r == Result::Ok) {
            CHECK_EQ(f.caf.reservations.size, 3);
            CHECK(ids[0] > 0 && ids[1] > 0 && ids[2] > 0);
        } else {
            CHECK_EQ(r, Result::CutoffPassed);
            CHECK_EQ(f.caf.reservations.size, 0);
        }
        f.caf.clock = &f.clock;
    }
}
