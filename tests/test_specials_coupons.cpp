// Teachers' specials, staff-made coupons and the staff order list.
#include "testing.h"
#include "cafe_fixture.h"
#include "storage/storage.h"
#include <filesystem>

namespace fs = std::filesystem;
static const char* STAFF = "staff1";
static const char* TEACHER = "teacher@uni.example";

static void prices(CafeFixture& f) {
    f.caf.menu.items[KHICHURI].priceTaka = 80;
    f.caf.menu.items[FRIED_RICE].priceTaka = 100;
    f.caf.menu.items[TEHARI].priceTaka = 120;
}

static Coupon coupon(const char* code, DiscountKind kind, int value, const char* audience = "everyone") {
    Coupon c;
    c.code = code;
    c.occasion = "Teachers Day";
    c.kind = kind;
    c.value = value;
    c.audience = audience;
    c.fromDay = 0;
    c.toDay = 0;
    c.maxUses = 0;
    c.uses = 0;
    c.active = true;
    return c;
}

TEST(teachers_special_refuses_students_and_takes_teachers) {
    CafeFixture f;
    bool rec = false;
    CHECK_EQ(setTeachersOnly(f.caf, STAFF, TEHARI, true, rec), Result::Ok);
    CHECK(rec);
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, ALICE, TEHARI, 1, f.out, &r), Result::TeachersOnly);
    CHECK_EQ(reserve(f.caf, TEACHER, TEHARI, 1, f.out, &r, true), Result::Ok);
    TrayLine lines[] = {{KHICHURI, 1}, {TEHARI, 1}};
    int failed = -1, ids[2] = {0, 0};
    CHECK_EQ(reserveBatch(f.caf, BOB, lines, 2, f.out, failed, ids), Result::TeachersOnly);
    CHECK_EQ(failed, 1);
}

TEST(teachers_special_can_be_undone_and_is_fresh_only) {
    CafeFixture f;
    bool rec = false;
    CHECK(setTeachersOnly(f.caf, STAFF, LAYS, true, rec) != Result::Ok);  // packaged items are for everyone
    CHECK_EQ(setTeachersOnly(f.caf, STAFF, KHICHURI, true, rec), Result::Ok);
    StaffAction undone;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, undone), Result::Ok);
    CHECK(!f.caf.menu.items[KHICHURI].teachersOnly);
}

TEST(teachers_waitlist_for_a_special) {
    CafeFixture f;
    bool rec = false;
    setTeachersOnly(f.caf, STAFF, FRIED_RICE, true, rec);
    for (int i = 0; i < 4; i++) {  // 4 teachers x 2 = the daily limit of 8
        Reservation* r = nullptr;
        std::string who = "t" + std::to_string(i) + "@uni.example";
        CHECK_EQ(reserve(f.caf, who, FRIED_RICE, 2, f.out, &r, true), Result::Ok);
    }
    int pos = 0;
    CHECK_EQ(joinWaitlist(f.caf, ALICE, FRIED_RICE, 1, pos), Result::TeachersOnly);
    CHECK_EQ(joinWaitlist(f.caf, "t9@uni.example", FRIED_RICE, 1, pos, true), Result::Ok);
}

TEST(coupon_checks_code_audience_and_dates) {
    CafeFixture f;
    CHECK_EQ(addCoupon(f.caf, STAFF, coupon("ab", DiscountKind::Percent, 10)), Result::InvalidValue);  // too short
    CHECK_EQ(addCoupon(f.caf, STAFF, coupon("EID10", DiscountKind::Percent, 101)), Result::InvalidValue);
    CHECK_EQ(addCoupon(f.caf, STAFF, coupon("EID10", DiscountKind::Percent, 10, "alumni")), Result::InvalidValue);
    CHECK_EQ(addCoupon(f.caf, STAFF, coupon("eid10", DiscountKind::Percent, 10)), Result::Ok);
    CHECK_EQ(f.caf.coupons[0].code, std::string("EID10"));  // codes are kept in capitals
    CHECK_EQ(addCoupon(f.caf, STAFF, coupon("EID10", DiscountKind::Fixed, 20)), Result::CouponExists);
    CHECK_EQ(addCoupon(f.caf, STAFF, coupon("TEACH50", DiscountKind::Fixed, 50, "teacher")), Result::Ok);

    int d = 0;
    CHECK_EQ(checkCoupon(f.caf, "nope", "student", ALICE, 200, d), Result::BadCoupon);
    CHECK_EQ(checkCoupon(f.caf, " eid10 ", "student", ALICE, 200, d), Result::Ok);
    CHECK_EQ(d, 20);
    CHECK_EQ(checkCoupon(f.caf, "TEACH50", "student", ALICE, 200, d), Result::CouponNotForYou);
    CHECK_EQ(checkCoupon(f.caf, "TEACH50", "teacher", TEACHER, 30, d), Result::Ok);
    CHECK_EQ(d, 30);  // never more than the order

    Coupon later = coupon("NEXTWEEK", DiscountKind::Percent, 5);
    later.fromDay = 20261030;
    CHECK_EQ(addCoupon(f.caf, STAFF, later), Result::Ok);
    CHECK_EQ(checkCoupon(f.caf, "NEXTWEEK", "student", ALICE, 100, d), Result::CouponNotToday);
    Coupon backwards = coupon("BACK", DiscountKind::Percent, 5);
    backwards.fromDay = 20261030;
    backwards.toDay = 20261020;
    CHECK_EQ(addCoupon(f.caf, STAFF, backwards), Result::InvalidValue);

    CHECK_EQ(setCouponActive(f.caf, STAFF, "eid10", false), Result::Ok);
    CHECK_EQ(checkCoupon(f.caf, "EID10", "student", ALICE, 200, d), Result::BadCoupon);
    const Coupon* list[MAX_COUPONS];
    CHECK_EQ(couponsForToday(f.caf, "teacher", list, MAX_COUPONS), 1);  // TEACH50 only
    CHECK_EQ(removeCoupon(f.caf, STAFF, "TEACH50"), Result::Ok);
    CHECK_EQ(f.caf.couponCount, 2);
    CHECK_EQ(couponsForToday(f.caf, "teacher", list, MAX_COUPONS), 0);
}

TEST(tray_with_a_coupon_splits_the_discount) {
    CafeFixture f;
    prices(f);
    addCoupon(f.caf, STAFF, coupon("EID10", DiscountKind::Percent, 10));
    TrayLine lines[] = {{TEHARI, 2}, {KHICHURI, 1}};  // 240 + 80 = 320, 10 % = 32
    int failed = -1, ids[2] = {0, 0};
    OrderContext ctx;
    ctx.coupon = "eid10";
    nlClear(f.out);
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 2, f.out, failed, ids, ctx), Result::Ok);
    Reservation* a = rlFindById(f.caf.reservations, ids[0]);
    Reservation* b = rlFindById(f.caf.reservations, ids[1]);
    CHECK_EQ(a->priceTaka, 120);
    CHECK_EQ(a->discountTaka + b->discountTaka, 32);
    CHECK_EQ(a->coupon, std::string("EID10"));
    CHECK_EQ(f.caf.coupons[0].uses, 1);
    CHECK(f.out.head->body.find("Coupon EID10") != std::string::npos);

    // once a day per person
    TrayLine again[] = {{FRIED_RICE, 1}};
    CHECK_EQ(reserveBatch(f.caf, ALICE, again, 1, f.out, failed, ids, ctx), Result::CouponAlreadyUsed);
    CHECK_EQ(f.caf.coupons[0].uses, 1);
}

TEST(a_refused_coupon_books_nothing) {
    CafeFixture f;
    prices(f);
    Coupon c = coupon("ONCE", DiscountKind::Fixed, 20);
    c.maxUses = 1;
    addCoupon(f.caf, STAFF, c);
    TrayLine lines[] = {{TEHARI, 1}};
    int failed = -1, ids[1] = {0};
    OrderContext ctx;
    ctx.coupon = "ONCE";
    CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 1, f.out, failed, ids, ctx), Result::Ok);
    CHECK_EQ(reserveBatch(f.caf, BOB, lines, 1, f.out, failed, ids, ctx), Result::CouponUsedUp);
    CHECK_EQ(f.caf.reservations.size, 1);
    CHECK_EQ(f.caf.nextToken, 2);  // the refused order took no token
}

TEST(cancelling_the_whole_order_gives_the_coupon_back) {
    CafeFixture f;
    prices(f);
    addCoupon(f.caf, STAFF, coupon("EID10", DiscountKind::Percent, 10));
    TrayLine lines[] = {{TEHARI, 1}, {KHICHURI, 1}};
    int failed = -1, ids[2] = {0, 0};
    OrderContext ctx;
    ctx.coupon = "EID10";
    reserveBatch(f.caf, ALICE, lines, 2, f.out, failed, ids, ctx);
    CHECK_EQ(cancelReservation(f.caf, ALICE, ids[0], f.out), Result::Ok);
    CHECK_EQ(f.caf.coupons[0].uses, 1);  // one dish still ordered
    CHECK_EQ(cancelReservation(f.caf, ALICE, ids[1], f.out), Result::Ok);
    CHECK_EQ(f.caf.coupons[0].uses, 0);
    int d = 0;
    CHECK_EQ(checkCoupon(f.caf, "EID10", "student", ALICE, 100, d), Result::Ok);  // usable again today
}

TEST(staff_order_list_is_newest_token_first) {
    CafeFixture f;
    f.reserveOk(ALICE, TEHARI, 1);  // token 1
    TrayLine lines[] = {{KHICHURI, 1}, {FRIED_RICE, 1}};
    int failed = -1, ids[2] = {0, 0};
    reserveBatch(f.caf, BOB, lines, 2, f.out, failed, ids);  // token 2, two dishes
    Reservation* out[10];
    int n = staffOrdersSorted(f.caf, out, 10);
    CHECK_EQ(n, 3);
    CHECK_EQ(out[0]->token, 2);
    CHECK_EQ(out[1]->token, 2);
    CHECK(out[0]->id < out[1]->id);
    CHECK_EQ(out[2]->token, 1);
}

TEST(specials_and_coupons_survive_a_restart) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests" / "coupons";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::string err;
    time_t savedAt;
    {
        CafeFixture f;
        prices(f);
        bool rec = false;
        setTeachersOnly(f.caf, STAFF, TEHARI, true, rec);
        Coupon c = coupon("EID10", DiscountKind::Percent, 10, "student");
        c.toDay = 20261130;
        c.maxUses = 50;
        addCoupon(f.caf, STAFF, c);
        TrayLine lines[] = {{KHICHURI, 1}};
        int failed = -1, ids[1] = {0};
        OrderContext ctx;
        ctx.coupon = "EID10";
        CHECK_EQ(reserveBatch(f.caf, ALICE, lines, 1, f.out, failed, ids, ctx), Result::Ok);
        CHECK(saveCafeteria(f.caf, dir.string(), err));
        savedAt = f.clock.now();
    }
    FakeClock clock;
    clock.t = savedAt;
    Cafeteria caf;
    cafInit(caf, &clock, 100);
    CHECK(loadCafeteria(caf, dir.string(), "", err));
    CHECK(caf.menu.items[TEHARI].teachersOnly);
    CHECK_EQ(caf.couponCount, 1);
    if (caf.couponCount == 1) {
        CHECK_EQ(caf.coupons[0].uses, 1);
        CHECK_EQ(caf.coupons[0].maxUses, 50);
        CHECK_EQ(caf.coupons[0].toDay, 20261130);
        CHECK_EQ(caf.coupons[0].audience, std::string("student"));
    }
    CHECK(caf.reservations.head != nullptr);
    if (caf.reservations.head) {
        CHECK_EQ(caf.reservations.head->priceTaka, 80);
        CHECK_EQ(caf.reservations.head->discountTaka, 8);
        CHECK_EQ(caf.reservations.head->coupon, std::string("EID10"));
    }
    cafFree(caf);
}
