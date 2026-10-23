#include "testing.h"
#include "cafe_fixture.h"
#include "storage/storage.h"
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static const char* STAFF = "staff1";

// A fresh, empty directory under the system temp folder.
static std::string tempDir(const char* name) {
    fs::path p = fs::temp_directory_path() / "cafeteria_tests" / name;
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

static std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::string s, line;
    while (std::getline(in, line)) s += line + "\n";
    return s;
}

// Loads a second Cafeteria from `dir`, as a restarted server would.
struct Restarted {
    FakeClock clock;
    Cafeteria caf;
    std::string err;
    bool ok;
    Restarted(const std::string& dir, time_t now, const std::string& seed = "") {
        clock.t = now;
        cafInit(caf, &clock, 100);
        ok = loadCafeteria(caf, dir, seed, err);
    }
    ~Restarted() { cafFree(caf); }
};

TEST(round_trip_menu_settings_reservations) {
    std::string dir = tempDir("round_trip");
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 2);
    f.reserveOk(BOB, KHICHURI, 1);
    bool rec;
    setStock(f.caf, STAFF, LAYS, 33, rec);
    changeSetting(f.caf, STAFF, "freshness_minutes", 480, rec);
    std::string err;
    CHECK(saveCafeteria(f.caf, dir, err));

    Restarted r(dir, f.clock.now());
    CHECK(r.ok);
    CHECK_EQ(r.err, std::string(""));
    CHECK_EQ(r.caf.menu.count, 4);
    CHECK_EQ(r.caf.menu.items[TEHARI].name, std::string("Tehari"));
    CHECK_EQ(r.caf.menu.items[TEHARI].reservedToday, 2);  // recomputed from reservations
    CHECK_EQ(r.caf.menu.items[LAYS].stock, 33);
    CHECK_EQ(r.caf.menu.items[LAYS].kind, ItemKind::Packaged);
    CHECK_EQ(r.caf.settings.freshnessMinutes, 480);
    CHECK_EQ(r.caf.reservations.size, 2);
    Reservation* a = rlFindById(r.caf.reservations, 1);
    CHECK(a != nullptr);
    CHECK_EQ(a->user, std::string(ALICE));
    CHECK_EQ(a->quantity, 2);
    CHECK_EQ(a->status, ResStatus::Reserved);
    CHECK_EQ(a->reservedAt, f.clock.now());
}

TEST(restart_rebuilds_cooking_queue_skipping_cancelled) {
    std::string dir = tempDir("cooking");
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    Reservation* b = f.reserveOk(BOB, TEHARI, 1);
    f.reserveOk(CAROL, TEHARI, 1);
    cancelReservation(f.caf, BOB, b->id, f.out);
    std::string err;
    saveCafeteria(f.caf, dir, err);

    Restarted r(dir, f.clock.now());
    CHECK_EQ(r.caf.cooking[TEHARI].count, 2);
    CHECK_EQ(qAt(r.caf.cooking[TEHARI], 0)->user, std::string(ALICE));
    CHECK_EQ(qAt(r.caf.cooking[TEHARI], 1)->user, std::string(CAROL));
    CHECK_EQ(r.caf.menu.items[TEHARI].reservedToday, 2);
}

TEST(restart_rebuilds_reminder_queue_in_due_order) {
    std::string dir = tempDir("reminders");
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    f.reserveOk(BOB, KHICHURI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, KHICHURI, 0, f.out, rep);  // 09:00 bob
    f.clock.advanceMinutes(20);
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);    // 09:20 alice
    f.clock.advanceMinutes(50);                         // 10:10
    timerTick(f.caf, f.out);                            // bob reminded at 10:00 -> moves to rear
    std::string err;
    saveCafeteria(f.caf, dir, err);

    Restarted r(dir, f.clock.now());
    CHECK_EQ(r.caf.reminders.count, 2);
    CHECK_EQ(qAt(r.caf.reminders, 0)->user, std::string(ALICE));  // next due 10:20
    CHECK_EQ(qAt(r.caf.reminders, 1)->user, std::string(BOB));    // next due 11:00

    NotificationList out;
    nlInit(out);
    r.clock.t += 10 * 60;  // 10:20
    timerTick(r.caf, out);
    CHECK_EQ(out.count, 1);
    CHECK_EQ(out.head->to, std::string(ALICE));
    CHECK_EQ(out.head->type, NotifType::Reminder);  // no duplicate "ready" email
    nlClear(out);
}

TEST(restart_stack_is_empty) {
    std::string dir = tempDir("stack");
    CafeFixture f;
    bool rec;
    setStock(f.caf, STAFF, LAYS, 5, rec);
    std::string err;
    saveCafeteria(f.caf, dir, err);
    Restarted r(dir, f.clock.now());
    CHECK(stIsEmpty(r.caf.history));
    CHECK_EQ(r.caf.menu.items[LAYS].stock, 5);
}

TEST(ids_continue_after_restart) {
    std::string dir = tempDir("ids");
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    f.reserveOk(BOB, TEHARI, 1);
    std::string err;
    saveCafeteria(f.caf, dir, err);
    Restarted r(dir, f.clock.now());
    CHECK_EQ(r.caf.reservations.nextId, 3);
}

TEST(ids_continue_after_archive) {
    std::string dir = tempDir("ids_archive");
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    cancelReservation(f.caf, ALICE, a->id, f.out);
    f.clock.set(2026, 10, 24, 0, 1, 0);
    timerTick(f.caf, f.out);  // rollover moves R-0001 to the archive
    std::string err;
    CHECK(saveCafeteria(f.caf, dir, err));
    CHECK_EQ(f.caf.archive.size, 0);  // flushed
    CHECK(readFile(dir + "/history.txt").find("1|alice@uni.example|2|1|") != std::string::npos);
    Restarted r(dir, f.clock.now());
    CHECK_EQ(r.caf.reservations.size, 0);
    CHECK_EQ(r.caf.reservations.nextId, 2);  // R-0001 is not reused
}

TEST(ready_item_expired_during_downtime_expires_on_first_tick) {
    std::string dir = tempDir("downtime");
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);  // 09:00, fresh until 16:00
    std::string err;
    saveCafeteria(f.caf, dir, err);

    f.clock.set(2026, 10, 23, 18, 0, 0);  // server comes back at 18:00
    Restarted r(dir, f.clock.now());
    NotificationList out;
    nlInit(out);
    timerTick(r.caf, out);
    CHECK_EQ(out.count, 1);
    CHECK_EQ(out.head->type, NotifType::Expired);
    CHECK_EQ(rlFindById(r.caf.reservations, a->id)->status, ResStatus::Expired);
    nlClear(out);
    timerTick(r.caf, out);
    CHECK_EQ(out.count, 0);
}

TEST(restart_next_day_runs_rollover) {
    std::string dir = tempDir("next_day");
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    std::string err;
    saveCafeteria(f.caf, dir, err);
    f.clock.set(2026, 10, 24, 8, 0, 0);
    Restarted r(dir, f.clock.now());
    NotificationList out;
    nlInit(out);
    timerTick(r.caf, out);
    CHECK_EQ(r.caf.currentDay, 20261024);
    CHECK(rlFindById(r.caf.archive, a->id) != nullptr);
    CHECK_EQ(rlFindById(r.caf.archive, a->id)->status, ResStatus::Expired);
    CHECK_EQ(r.caf.menu.items[TEHARI].reservedToday, 0);
    nlClear(out);
}

TEST(corrupt_line_reports_line_number_and_refuses_start) {
    std::string dir = tempDir("corrupt");
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    std::string err;
    saveCafeteria(f.caf, dir, err);
    {
        std::ofstream o(dir + "/reservations.txt", std::ios::app);
        o << "2|bob@uni.example|2|notanumber|0|Reserved|0|0|20261023\n";
    }
    Restarted r(dir, f.clock.now());
    CHECK(!r.ok);
    CHECK(r.err.find("reservations.txt line") != std::string::npos);
}

TEST(unknown_item_index_in_reservations_refused) {
    std::string dir = tempDir("bad_index");
    CafeFixture f;
    std::string err;
    saveCafeteria(f.caf, dir, err);
    {
        std::ofstream o(dir + "/reservations.txt", std::ios::app);
        o << "1|bob@uni.example|29|1|0|Reserved|0|0|20261023\n";
    }
    Restarted r(dir, f.clock.now());
    CHECK(!r.ok);
}

TEST(missing_files_start_from_seed) {
    std::string dir = tempDir("from_seed");
    std::string seed = tempDir("seed_src");
    {
        std::ofstream o(seed + "/menu.txt");
        o << "# category|name|kind|stock|dailyLimit\n"
          << "Fresh food|Khichuri|fresh|0|12\n"
          << "\n"
          << "Chips|Lays|packaged|0|0\n";
    }
    FakeClock c;
    c.set(2026, 10, 23, 9, 0, 0);
    Restarted r(dir, c.now(), seed);
    CHECK(r.ok);
    CHECK_EQ(r.caf.menu.count, 2);
    CHECK_EQ(r.caf.menu.items[0].dailyLimit, 12);
    CHECK_EQ(r.caf.menu.items[1].kind, ItemKind::Packaged);
    CHECK_EQ(r.caf.reservations.size, 0);
    CHECK_EQ(r.caf.settings.cutoffMinute, 600);
}

TEST(save_is_atomic_no_tmp_left_behind) {
    std::string dir = tempDir("atomic");
    CafeFixture f;
    std::string err;
    CHECK(saveCafeteria(f.caf, dir, err));
    CHECK(saveCafeteria(f.caf, dir, err));  // replacing existing files works
    CHECK(!fs::exists(dir + "/menu.txt.tmp"));
    CHECK(fs::exists(dir + "/menu.txt"));
}

TEST(ready_deadline_and_interval_survive_restart) {
    std::string dir = tempDir("deadline");
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    bool rec;
    changeSetting(f.caf, STAFF, "freshness_minutes", 60, rec);  // after ready: must not apply to a
    std::string err;
    CHECK(saveCafeteria(f.caf, dir, err));
    Restarted r(dir, f.clock.now());
    Reservation* b = rlFindById(r.caf.reservations, a->id);
    CHECK(b != nullptr);
    if (b == nullptr) return;
    CHECK_EQ(b->expiresAt, a->expiresAt);
    CHECK_EQ(b->reminderMinutes, 60);
}

TEST(old_nine_field_reservation_lines_still_load) {
    std::string dir = tempDir("old_format");
    CafeFixture f;
    std::string err;
    saveCafeteria(f.caf, dir, err);
    {
        std::ofstream o(dir + "/reservations.txt");
        o << "1|alice@uni.example|2|1|1000|Ready|2000|2000|20261023\n";
    }
    Restarted r(dir, f.clock.now());
    CHECK(r.ok);
    Reservation* a = rlFindById(r.caf.reservations, 1);
    CHECK(a != nullptr);
    if (a == nullptr) return;
    CHECK_EQ(a->expiresAt, (time_t)(2000 + 420 * 60));  // filled in from the settings
    CHECK_EQ(a->reminderMinutes, 60);
}
