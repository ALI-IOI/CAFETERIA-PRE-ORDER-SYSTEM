#include "testing.h"
#include "cafe_fixture.h"

static const char* STAFF = "staff1";

// Notifications of one type, in the order they were queued.
static int collectTo(const NotificationList& out, NotifType t, std::string* to, int cap) {
    int n = 0;
    for (Notification* x = out.head; x && n < cap; x = x->next)
        if (x->type == t) to[n++] = x->to;
    return n;
}

static void tickMinutes(CafeFixture& f, int minutes) {
    for (int i = 0; i < minutes; i++) {
        f.clock.advanceMinutes(1);
        timerTick(f.caf, f.out);
    }
}

TEST(ready_emails_in_fifo_order) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    f.reserveOk(BOB, TEHARI, 1);
    f.reserveOk(CAROL, TEHARI, 1);
    nlClear(f.out);
    ReadyReport rep;
    CHECK_EQ(markReady(f.caf, STAFF, TEHARI, 0, f.out, rep), Result::Ok);
    CHECK_EQ(rep.madeReady, 3);
    std::string to[5];
    CHECK_EQ(collectTo(f.out, NotifType::Ready, to, 5), 3);
    CHECK_EQ(to[0], std::string(ALICE));
    CHECK_EQ(to[1], std::string(BOB));
    CHECK_EQ(to[2], std::string(CAROL));
    CHECK(qIsEmpty(f.caf.cooking[TEHARI]));
    CHECK_EQ(f.caf.reminders.count, 3);
    CHECK(f.out.head->body.find("Your Tehari (R-0001) is ready for collection.") != std::string::npos);
    CHECK(f.out.head->body.find("about 7 hours") != std::string::npos);
}

TEST(ready_skips_cancelled) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    Reservation* b = f.reserveOk(BOB, TEHARI, 1);
    f.reserveOk(CAROL, TEHARI, 1);
    cancelReservation(f.caf, BOB, b->id, f.out);
    nlClear(f.out);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    CHECK_EQ(rep.madeReady, 2);
    CHECK_EQ(rep.skipped, 1);
    std::string to[5];
    CHECK_EQ(collectTo(f.out, NotifType::Ready, to, 5), 2);
    CHECK_EQ(to[0], std::string(ALICE));
    CHECK_EQ(to[1], std::string(CAROL));
    CHECK_EQ(b->status, ResStatus::Cancelled);
}

TEST(ready_batch_stops_at_portion_count) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 2);
    Reservation* b = f.reserveOk(BOB, TEHARI, 2);
    ReadyReport rep;
    CHECK_EQ(markReady(f.caf, STAFF, TEHARI, 3, f.out, rep), Result::Ok);
    CHECK_EQ(rep.madeReady, 1);
    CHECK_EQ(a->status, ResStatus::Ready);
    CHECK_EQ(b->status, ResStatus::Reserved);
    CHECK(qPeek(f.caf.cooking[TEHARI]) == b);  // still first in line for the next batch
    CHECK_EQ(rep.leftWaiting, 1);
}

TEST(ready_on_empty_queue_returns_0) {
    CafeFixture f;
    ReadyReport rep;
    CHECK_EQ(markReady(f.caf, STAFF, TEHARI, 0, f.out, rep), Result::Ok);
    CHECK_EQ(rep.madeReady, 0);
    CHECK_EQ(f.out.count, 0);
    CHECK_EQ(markReady(f.caf, STAFF, LAYS, 0, f.out, rep), Result::NotFresh);
    CHECK_EQ(markReady(f.caf, STAFF, 77, 0, f.out, rep), Result::BadItem);
    CHECK_EQ(markReady(f.caf, STAFF, TEHARI, -1, f.out, rep), Result::InvalidValue);
}

TEST(no_reminder_before_interval) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    nlClear(f.out);
    tickMinutes(f, 59);
    CHECK_EQ(f.countType(NotifType::Reminder), 0);
}

TEST(reminder_at_interval_and_requeued) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    nlClear(f.out);
    tickMinutes(f, 60);
    CHECK_EQ(f.countType(NotifType::Reminder), 1);
    CHECK(f.out.head->body.find("Reminder: your Tehari (R-0001) is ready. About 6 hours of freshness left.") !=
          std::string::npos);
    CHECK_EQ(f.caf.reminders.count, 1);  // re-enqueued at the rear
    CHECK_EQ(a->lastNoticeAt, a->readyAt + 3600);
}

TEST(seven_hour_window_gives_6_reminders_then_expiry) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    f.clock.set(2026, 10, 23, 11, 0, 0);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    nlClear(f.out);
    tickMinutes(f, 7 * 60 + 30);
    CHECK_EQ(f.countType(NotifType::Reminder), 6);
    CHECK_EQ(f.countType(NotifType::Expired), 1);
    CHECK_EQ(a->status, ResStatus::Expired);
    CHECK(qIsEmpty(f.caf.reminders));
    CHECK(f.out.tail->body.find("freshness window") != std::string::npos);
}

TEST(reminders_interleave_in_due_order_for_two_items) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    f.reserveOk(BOB, KHICHURI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);   // 09:00
    f.clock.advanceMinutes(20);
    markReady(f.caf, STAFF, KHICHURI, 0, f.out, rep); // 09:20
    nlClear(f.out);
    tickMinutes(f, 100);                               // until 11:00
    std::string to[10];
    CHECK_EQ(collectTo(f.out, NotifType::Reminder, to, 10), 3);
    CHECK_EQ(to[0], std::string(ALICE));  // 10:00
    CHECK_EQ(to[1], std::string(BOB));    // 10:20
    CHECK_EQ(to[2], std::string(ALICE));  // 11:00
}

TEST(collected_stops_reminders) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    bool recorded = false;
    CHECK_EQ(markCollected(f.caf, STAFF, a->id, recorded), Result::Ok);
    CHECK(recorded);
    CHECK_EQ(a->status, ResStatus::Collected);
    nlClear(f.out);
    tickMinutes(f, 8 * 60);
    CHECK_EQ(f.out.count, 0);
    CHECK(qIsEmpty(f.caf.reminders));  // lazily removed when it reached the front
}

TEST(collect_reserved_refused) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    bool recorded = true;
    CHECK_EQ(markCollected(f.caf, STAFF, a->id, recorded), Result::NotReady);
    CHECK(!recorded);
    CHECK_EQ(markCollected(f.caf, STAFF, 999, recorded), Result::NotFound);
    CHECK(stIsEmpty(f.caf.history));
}

TEST(rollover_expires_uncooked_and_resets_counts) {
    CafeFixture f;
    Reservation* waiting = f.reserveOk(ALICE, TEHARI, 1);
    Reservation* cancelled = f.reserveOk(BOB, KHICHURI, 1);
    Reservation* ready = f.reserveOk(CAROL, FRIED_RICE, 1);
    cancelReservation(f.caf, BOB, cancelled->id, f.out);
    f.clock.set(2026, 10, 23, 22, 0, 0);
    ReadyReport rep;
    markReady(f.caf, STAFF, FRIED_RICE, 0, f.out, rep);  // ready late, fresh until 05:00
    nlClear(f.out);
    int cancelledId = cancelled->id;

    f.clock.set(2026, 10, 24, 0, 1, 0);
    timerTick(f.caf, f.out);

    CHECK_EQ(f.caf.currentDay, 20261024);
    CHECK_EQ(waiting->status, ResStatus::Expired);
    CHECK_EQ(f.countType(NotifType::Expired), 0);  // never cooked: no email
    CHECK_EQ(f.caf.menu.items[TEHARI].reservedToday, 0);
    CHECK_EQ(f.caf.menu.items[FRIED_RICE].reservedToday, 0);
    CHECK(qIsEmpty(f.caf.cooking[TEHARI]));
    CHECK(rlFindById(f.caf.reservations, cancelledId) == nullptr);  // moved to the archive
    CHECK(rlFindById(f.caf.archive, cancelledId) != nullptr);
    CHECK_EQ(ready->status, ResStatus::Ready);                       // still within its window
    CHECK_EQ(f.caf.reminders.count, 1);
    // yesterday's portions do not count against today's per-user limit
    f.clock.set(2026, 10, 24, 8, 0, 0);
    CHECK(f.reserveOk(ALICE, TEHARI, 2) != nullptr);
}

// Review finding 1: a reservation made after midnight but before the first
// timer tick must not be wiped by the rollover that tick would run.
TEST(reserve_after_midnight_before_tick_is_kept) {
    CafeFixture f;
    f.caf.menu.items[TEHARI].dailyLimit = 2;
    f.reserveOk(ALICE, TEHARI, 2);                 // yesterday: sold out
    f.clock.set(2026, 10, 24, 0, 0, 20);            // 20 s after midnight, no tick yet
    Reservation* r = f.reserveOk(BOB, TEHARI, 1);   // must not see yesterday's count
    CHECK(r != nullptr);
    if (r == nullptr) return;
    CHECK_EQ(f.caf.menu.items[TEHARI].reservedToday, 1);
    timerTick(f.caf, f.out);                        // the first tick of the day
    CHECK_EQ(f.caf.menu.items[TEHARI].reservedToday, 1);
    CHECK_EQ(f.caf.cooking[TEHARI].count, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    CHECK_EQ(rep.madeReady, 1);
    CHECK_EQ(r->status, ResStatus::Ready);
}

// Review finding 3 / decision D7: settings changed after food is Ready apply
// only to food marked ready later. The deadline in the "ready" email holds.
TEST(freshness_change_after_ready_keeps_original_deadline) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    f.clock.set(2026, 10, 23, 11, 0, 0);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);          // fresh until 18:00, hourly
    CHECK_EQ(a->expiresAt, a->readyAt + 7 * 3600);
    bool rec;
    f.clock.advanceMinutes(5);
    changeSetting(f.caf, STAFF, "freshness_minutes", 60, rec);  // would mean 12:00
    changeSetting(f.caf, STAFF, "reminder_minutes", 30, rec);
    nlClear(f.out);
    tickMinutes(f, 7 * 60 - 6);                                 // until 17:59
    CHECK_EQ(a->status, ResStatus::Ready);
    CHECK_EQ(f.countType(NotifType::Reminder), 6);              // still hourly: 12..17
    CHECK_EQ(f.countType(NotifType::Expired), 0);
    tickMinutes(f, 1);                                          // 18:00
    CHECK_EQ(a->status, ResStatus::Expired);
    // food made ready after the change uses the new values
    Reservation* b = f.reserveOk(BOB, KHICHURI, 1);
    if (b == nullptr) {
        f.caf.settings.cutoffMinute = 23 * 60;
        b = f.reserveOk(BOB, KHICHURI, 1);
    }
    CHECK(b != nullptr);
    if (b == nullptr) return;
    markReady(f.caf, STAFF, KHICHURI, 0, f.out, rep);
    CHECK_EQ(b->expiresAt, b->readyAt + 3600);
    CHECK_EQ(b->reminderMinutes, 30);
}
