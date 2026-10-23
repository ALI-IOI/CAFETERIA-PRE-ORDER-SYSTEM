#include "testing.h"
#include "cafe_fixture.h"
#include "storage/history.h"
#include "storage/storage.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string historyDir(const char* name) {
    fs::path p = fs::temp_directory_path() / "cafeteria_tests" / name;
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

TEST(history_loads_only_that_users_orders_newest_first) {
    std::string dir = historyDir("hist_user");
    {
        std::ofstream o(dir + "/history.txt");
        o << "1|alice@uni.example|2|1|1000|Collected|2000|2000|20261021|3000|60\n"
          << "2|bob@uni.example|0|1|1100|Expired|0|0|20261021|0|0\n"
          << "3|alice@uni.example|0|2|5000|Cancelled|0|0|20261022\n";  // old 9-field line
    }
    ReservationList l;
    rlInit(l);
    std::string err;
    CHECK(loadUserHistory(dir, "alice@uni.example", l, err));
    CHECK_EQ(l.size, 2);
    CHECK_EQ(l.head->id, 3);  // 22nd before 21st
    CHECK_EQ(l.head->status, ResStatus::Cancelled);
    CHECK_EQ(l.tail->id, 1);
    rlClear(l);
}

TEST(history_missing_file_is_empty) {
    std::string dir = historyDir("hist_none");
    ReservationList l;
    rlInit(l);
    std::string err;
    CHECK(loadUserHistory(dir, "alice@uni.example", l, err));
    CHECK_EQ(l.size, 0);
}

// One torn or damaged line (say, a crash during the midnight append) must not
// hide everyone's history: it is skipped and reported, the rest still loads.
TEST(history_bad_line_skipped_and_reported) {
    std::string dir = historyDir("hist_bad");
    {
        std::ofstream o(dir + "/history.txt");
        o << "x|alice@uni.example|2|1|1000|Collected|0|0|20261021\n";
        o << "GARBAGE LINE\n";
        o << "7|alice@uni.example|2|1|1000|Collected|0|0|20261021\n";
    }
    ReservationList l;
    rlInit(l);
    std::string err;
    CHECK(loadUserHistory(dir, "alice@uni.example", l, err));
    CHECK_EQ(l.size, 1);
    CHECK(l.head != nullptr && l.head->id == 7);
    CHECK(err.find("history.txt line 1") != std::string::npos);
    CHECK(err.find("2 damaged") != std::string::npos);
    rlClear(l);
}

TEST(history_after_rollover_contains_yesterdays_orders) {
    std::string dir = historyDir("hist_rollover");
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    cancelReservation(f.caf, ALICE, a->id, f.out);
    f.reserveOk(ALICE, KHICHURI, 1);  // never cooked: expires at rollover
    f.clock.set(2026, 10, 24, 0, 5, 0);
    timerTick(f.caf, f.out);
    std::string err;
    CHECK(saveCafeteria(f.caf, dir, err));  // archive appended to history.txt
    ReservationList l;
    rlInit(l);
    CHECK(loadUserHistory(dir, ALICE, l, err));
    CHECK_EQ(l.size, 2);
    rlClear(l);
}
