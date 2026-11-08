// Discount coupons, set up by cafeteria staff for an occasion (Pohela Boishakh,
// Teachers' Day, an exam week...): a code people type at the tray, a percentage
// or a fixed amount off the order, who may use it (everyone, students, teachers
// or university staff), the days it works and how many orders it may be used on.
//
// Kept in a fixed array (Cafeteria::coupons) in the order staff made them, like
// the menu; lookup by code is a linear search, O(c) for c coupons (at most 64).
// A person may use one coupon once a day: checked by walking today's
// reservations in the linked list, O(n).
#include "core/cafeteria.h"
#include "auth/validation.h"
#include <cctype>

std::string couponCodeNormal(const std::string& code) {
    size_t a = code.find_first_not_of(" \t"), b = code.find_last_not_of(" \t");
    if (a == std::string::npos) return "";
    std::string out = code.substr(a, b - a + 1);
    for (char& ch : out) ch = (char)std::toupper((unsigned char)ch);
    return out;
}

static bool validCode(const std::string& code) {
    if (code.size() < 3 || code.size() > 16) return false;
    for (char ch : code)
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) return false;
    return true;
}

static bool validAudience(const std::string& a) {
    return a == "everyone" || a == "student" || a == "teacher" || a == "employee";
}

static bool validDay(int day) {
    if (day == 0) return true;
    int y = day / 10000, m = day / 100 % 100, d = day % 100;
    return y >= 2000 && y <= 2100 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}

int findCoupon(const Cafeteria& caf, const std::string& code) {
    const std::string c = couponCodeNormal(code);
    for (int i = 0; i < caf.couponCount; i++)
        if (caf.coupons[i].code == c) return i;
    return -1;
}

std::string couponText(const Coupon& c) {
    if (c.kind == DiscountKind::Percent) return std::to_string(c.value) + "% off";
    return "\xE0\xA7\xB3 " + std::to_string(c.value) + " off";  // "৳ 50 off"
}

Result addCoupon(Cafeteria& caf, const std::string& staff, Coupon c) {
    (void)staff;
    c.code = couponCodeNormal(c.code);
    if (!validCode(c.code) || !validName(c.occasion) || !validAudience(c.audience)) return Result::InvalidValue;
    if (c.kind == DiscountKind::Percent ? (c.value < 1 || c.value > 100) : (c.value < 1 || c.value > MAX_PRICE_TAKA))
        return Result::InvalidValue;
    if (!validDay(c.fromDay) || !validDay(c.toDay) || (c.fromDay && c.toDay && c.toDay < c.fromDay))
        return Result::InvalidValue;
    if (c.maxUses < 0 || c.maxUses > 100000) return Result::InvalidValue;
    if (findCoupon(caf, c.code) >= 0) return Result::CouponExists;
    if (caf.couponCount >= MAX_COUPONS) return Result::CouponsFull;
    c.uses = 0;
    caf.coupons[caf.couponCount++] = c;
    return Result::Ok;
}

Result setCouponActive(Cafeteria& caf, const std::string& staff, const std::string& code, bool active) {
    (void)staff;
    int i = findCoupon(caf, code);
    if (i < 0) return Result::BadCoupon;
    caf.coupons[i].active = active;
    return Result::Ok;
}

Result removeCoupon(Cafeteria& caf, const std::string& staff, const std::string& code) {
    (void)staff;
    int i = findCoupon(caf, code);
    if (i < 0) return Result::BadCoupon;
    for (int k = i; k + 1 < caf.couponCount; k++) caf.coupons[k] = caf.coupons[k + 1];  // keep the order: O(c)
    caf.couponCount--;
    return Result::Ok;
}

// Every check except "has this person used it today".
static Result usable(const Cafeteria& caf, const Coupon& c, const std::string& role) {
    if (!c.active) return Result::BadCoupon;
    const int today = caf.currentDay;
    if ((c.fromDay && today < c.fromDay) || (c.toDay && today > c.toDay)) return Result::CouponNotToday;
    if (c.audience != "everyone" && c.audience != role) return Result::CouponNotForYou;
    if (c.maxUses > 0 && c.uses >= c.maxUses) return Result::CouponUsedUp;
    return Result::Ok;
}

Result checkCoupon(const Cafeteria& caf, const std::string& code, const std::string& role, const std::string& user,
                   int subtotal, int& discount) {
    discount = 0;
    int i = findCoupon(caf, code);
    if (i < 0) return Result::BadCoupon;
    const Coupon& c = caf.coupons[i];
    Result r = usable(caf, c, role);
    if (r != Result::Ok) return r;
    for (const Reservation* res = caf.reservations.head; res != nullptr; res = res->next)
        if (res->user == user && res->day == caf.currentDay && res->coupon == c.code &&
            res->status != ResStatus::Cancelled)
            return Result::CouponAlreadyUsed;
    discount = c.kind == DiscountKind::Percent ? subtotal * c.value / 100 : c.value;
    if (discount > subtotal) discount = subtotal;
    if (discount < 0) discount = 0;
    return Result::Ok;
}

int couponsForToday(const Cafeteria& caf, const std::string& role, const Coupon** out, int cap) {
    int n = 0;
    for (int i = 0; i < caf.couponCount && n < cap; i++)
        if (usable(caf, caf.coupons[i], role) == Result::Ok) out[n++] = &caf.coupons[i];
    return n;
}
