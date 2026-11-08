// Order tokens: numbering, the token board, handing food over, days and restarts.
#include "testing.h"
#include "cafe_fixture.h"
#include "storage/storage.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static const char* STAFF = "staff1";

static int tokenOf(CafeFixture& f, int reservationId) {
    Reservation* r = rlFindById(f.caf.reservations, reservationId);
    return r ? r->token : -1;
}

static void tick(CafeFixture& f, int minutes) {
    f.clock.advanceMinutes(minutes);
    timerTick(f.caf, f.out);
}

TEST(token_text_has_three_digits) {
    CHECK_EQ(tokenText(7), std::string("007"));
    CHECK_EQ(tokenText(42), std::string("042"));
    CHECK_EQ(tokenText(1234), std::string("1234"));
}

TEST(each_order_gets_the_next_token) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    Reservation* b = f.reserveOk(BOB, TEHARI, 1);
    Reservation* c = f.reserveOk(ALICE, KHICHURI, 1);  // a second order of the same person
    CHECK_EQ(a->token, 1);
    CHECK_EQ(b->token, 2);
    CHECK_EQ(c->token, 3);
    CHECK_EQ(f.caf.nextToken, 4);
    CHECK(f.out.head->body.find("Your token is 001.") != std::string::npos);
}

TEST(a_tray_shares_one_token) {
    CafeFixture f;
    f.reserveOk(CAROL, FRIED_RICE, 1);  // token 1
    TrayLine lines[] = {{TEHARI, 2}, {KHICHURI, 1}};
    int failed = -1, ids[2] = {0, 0};
    nlClear(f.out);
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 2, f.out, failed, ids), Result::Ok);
    CHECK_EQ(tokenOf(f, ids[0]), 2);
    CHECK_EQ(tokenOf(f, ids[1]), 2);
    CHECK(f.out.head->body.find("Your token is 002.") != std::string::npos);
}

TEST(a_refused_order_uses_no_token) {
    CafeFixture f;
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, ALICE, LAYS, 1, f.out, &r), Result::NotFresh);
    TrayLine lines[] = {{TEHARI, 1}, {TEHARI, 9}};
    int failed = -1, ids[2] = {0, 0};
    CHECK(reserveBatch(f.caf, ALICE, lines, 2, f.out, failed, ids) != Result::Ok);
    CHECK_EQ(f.caf.nextToken, 1);
    CHECK_EQ(f.reserveOk(BOB, TEHARI, 1)->token, 1);
}

TEST(waitlist_promotion_gets_its_own_token) {
    CafeFixture f;
    const char* users[] = {"u1@uni.example", "u2@uni.example", "u3@uni.example", "u4@uni.example"};
    Reservation* first = nullptr;
    for (const char* u : users) {
        Reservation* r = f.reserveOk(u, FRIED_RICE, 2);  // 4 x 2 = the daily limit of 8
        if (!first) first = r;
    }
    int pos = 0;
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::Ok);
    CHECK_EQ(cancelReservation(f.caf, "u1@uni.example", first->id, f.out), Result::Ok);
    Reservation* promoted = f.caf.reservations.tail;
    CHECK_EQ(promoted->user, std::string(BOB));
    CHECK_EQ(promoted->token, 5);
}

TEST(board_splits_cooking_and_ready_in_calling_order) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);    // token 1
    f.reserveOk(BOB, KHICHURI, 1);    // token 2
    TrayLine lines[] = {{TEHARI, 1}, {FRIED_RICE, 1}};
    int failed = -1, ids[2] = {0, 0};
    reserveBatch(f.caf, CAROL, lines, 2, f.out, failed, ids);  // token 3: two dishes
    ReadyReport rep;
    tick(f, 5);
    markReady(f.caf, STAFF, KHICHURI, 0, f.out, rep);  // token 2 ready first
    tick(f, 5);
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);    // token 1 ready; token 3 half ready

    TokenInfo cooking[10], ready[10];
    int nc = 0, nr = 0;
    tokenBoard(f.caf, cooking, nc, ready, nr, 10);
    CHECK_EQ(nr, 2);
    CHECK_EQ(ready[0].token, 2);  // ready first, called first
    CHECK_EQ(ready[1].token, 1);
    CHECK_EQ(nc, 1);
    CHECK_EQ(cooking[0].token, 3);
    CHECK_EQ(cooking[0].ready, 1);
    CHECK_EQ(cooking[0].dishes, 2);

    tick(f, 5);
    markReady(f.caf, STAFF, FRIED_RICE, 0, f.out, rep);
    tokenBoard(f.caf, cooking, nc, ready, nr, 10);
    CHECK_EQ(nc, 0);
    CHECK_EQ(nr, 3);
    CHECK_EQ(ready[2].token, 3);
}

TEST(cancelled_orders_leave_the_board) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    f.reserveOk(BOB, TEHARI, 1);
    CHECK_EQ(cancelReservation(f.caf, ALICE, a->id, f.out), Result::Ok);
    TokenInfo all[10];
    int n = todaysTokens(f.caf, all, 10);
    CHECK_EQ(n, 1);
    CHECK_EQ(all[0].token, 2);
}

TEST(hand_over_collects_every_ready_dish_of_the_token) {
    CafeFixture f;
    TrayLine lines[] = {{TEHARI, 1}, {KHICHURI, 1}, {FRIED_RICE, 1}};
    int failed = -1, ids[3] = {0, 0, 0};
    reserveBatch(f.caf, ALICE, lines, 3, f.out, failed, ids);  // token 1
    int handed = 0;
    bool rec = false;
    CHECK_EQ(handOverToken(f.caf, STAFF, 9, handed, rec), Result::NoSuchToken);
    CHECK_EQ(handOverToken(f.caf, STAFF, 1, handed, rec), Result::TokenNotReady);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    markReady(f.caf, STAFF, KHICHURI, 0, f.out, rep);
    CHECK_EQ(handOverToken(f.caf, STAFF, 1, handed, rec), Result::Ok);
    CHECK_EQ(handed, 2);
    CHECK(rec);
    CHECK_EQ(rlFindById(f.caf.reservations, ids[0])->status, ResStatus::Collected);
    CHECK_EQ(rlFindById(f.caf.reservations, ids[2])->status, ResStatus::Reserved);  // still cooking
    TokenInfo t;
    CHECK(findToken(f.caf, 1, t));
    CHECK_EQ(tokenState(t), TokenState::Cooking);
    // undo brings back one dish at a time
    StaffAction undone;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, undone), Result::Ok);
    CHECK(findToken(f.caf, 1, t));
    CHECK_EQ(t.ready, 1);
}

TEST(hand_over_twice_says_already_handed_over) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    int handed = 0;
    bool rec = false;
    CHECK_EQ(handOverToken(f.caf, STAFF, 1, handed, rec), Result::Ok);
    CHECK_EQ(handOverToken(f.caf, STAFF, 1, handed, rec), Result::TokenDone);
    CHECK_EQ(handed, 0);
}

TEST(ready_email_names_the_token) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    nlClear(f.out);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    CHECK(f.out.head->body.find("Token 001.") == 0);
    CHECK(f.out.head->body.find("Your Tehari (R-0001) is ready for collection.") != std::string::npos);
}

TEST(tokens_start_again_the_next_day) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);
    f.reserveOk(BOB, TEHARI, 1);
    f.clock.set(2026, 10, 24, 8, 0, 0);
    Reservation* r = f.reserveOk(CAROL, TEHARI, 1);
    CHECK_EQ(r->token, 1);
    TokenInfo all[10];
    CHECK_EQ(todaysTokens(f.caf, all, 10), 1);  // yesterday's tokens are not today's
}

TEST(tokens_survive_a_restart_and_old_files_load) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests" / "tokens";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::string err;
    time_t savedAt;
    {
        CafeFixture f;
        f.reserveOk(ALICE, TEHARI, 1);
        f.reserveOk(BOB, KHICHURI, 1);
        CHECK(saveCafeteria(f.caf, dir.string(), err));
        savedAt = f.clock.now();
    }
    {
        FakeClock clock;
        clock.t = savedAt;
        Cafeteria caf;
        cafInit(caf, &clock, 100);
        CHECK(loadCafeteria(caf, dir.string(), "", err));
        CHECK_EQ(caf.nextToken, 3);  // the next order continues the day's numbering
        CHECK_EQ(caf.reservations.head->token, 1);
        CHECK_EQ(caf.reservations.tail->token, 2);
        cafFree(caf);
    }
    // a reservations file from before tokens (11 fields) still loads, with no token
    {
        std::ifstream in((dir / "reservations.txt").string());
        std::string all, line;
        while (std::getline(in, line)) {
            if (!line.empty() && line[0] != '#') {  // keep the first 11 fields: the format before tokens
                size_t cut = 0;
                for (int k = 0; k < 11 && cut != std::string::npos; k++) cut = line.find('|', cut + (k ? 1 : 0));
                line = line.substr(0, cut);
            }
            all += line + "\n";
        }
        in.close();
        std::ofstream((dir / "reservations.txt").string()) << all;
        FakeClock clock;
        clock.t = savedAt;
        Cafeteria caf;
        cafInit(caf, &clock, 100);
        CHECK(loadCafeteria(caf, dir.string(), "", err));
        CHECK(caf.reservations.head != nullptr);
        if (caf.reservations.head) CHECK_EQ(caf.reservations.head->token, 0);
        CHECK_EQ(caf.nextToken, 1);
        cafFree(caf);
    }
}
