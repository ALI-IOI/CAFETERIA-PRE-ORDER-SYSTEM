#include "testing.h"
#include "ds/reservation_list.h"

// Appends a reservation with a given day/time/ID (as loaded from history.txt).
static Reservation* add(ReservationList& l, int id, int day, time_t at) {
    Reservation* r = new Reservation;
    r->id = id;
    r->user = "u";
    r->itemIndex = 0;
    r->quantity = 1;
    r->reservedAt = at;
    r->status = ResStatus::Collected;
    r->readyAt = r->lastNoticeAt = r->expiresAt = 0;
    r->reminderMinutes = 0;
    r->day = day;
    rlAppendNode(l, r);
    return r;
}

static std::string order(const ReservationList& l) {
    std::string s;
    for (Reservation* r = l.head; r; r = r->next) s += std::to_string(r->id) + ",";
    return s;
}

TEST(sort_empty_and_single) {
    ReservationList l;
    rlInit(l);
    rlSortNewestFirst(l);
    CHECK(l.head == nullptr && l.tail == nullptr);
    add(l, 1, 20261020, 100);
    rlSortNewestFirst(l);
    CHECK_EQ(order(l), std::string("1,"));
    CHECK(l.tail == l.head);
    rlClear(l);
}

TEST(sort_newest_day_then_time_first) {
    ReservationList l;
    rlInit(l);
    add(l, 1, 20261020, 900);
    add(l, 2, 20261022, 100);
    add(l, 3, 20261021, 500);
    add(l, 4, 20261022, 300);
    add(l, 5, 20261020, 950);
    rlSortNewestFirst(l);
    CHECK_EQ(order(l), std::string("4,2,3,5,1,"));
    CHECK_EQ(l.tail->id, 1);
    CHECK(l.tail->next == nullptr);
    CHECK_EQ(l.size, 5);
    rlClear(l);
}

TEST(sort_ties_broken_by_higher_id_first) {
    ReservationList l;
    rlInit(l);
    add(l, 7, 20261022, 100);
    add(l, 9, 20261022, 100);
    add(l, 8, 20261022, 100);
    rlSortNewestFirst(l);
    CHECK_EQ(order(l), std::string("9,8,7,"));
    rlClear(l);
}

TEST(sort_already_sorted_and_reversed) {
    ReservationList a, b;
    rlInit(a);
    rlInit(b);
    for (int i = 1; i <= 50; i++) add(a, i, 20261001 + i, 0);          // oldest first
    for (int i = 50; i >= 1; i--) add(b, i, 20261001 + i, 0);          // newest first
    rlSortNewestFirst(a);
    rlSortNewestFirst(b);
    CHECK_EQ(order(a), order(b));
    CHECK_EQ(a.head->id, 50);
    CHECK_EQ(a.tail->id, 1);
    CHECK_EQ(b.tail->id, 1);
    rlClear(a);
    rlClear(b);
}

TEST(sort_large_list_keeps_every_node) {
    ReservationList l;
    rlInit(l);
    for (int i = 1; i <= 1000; i++) add(l, i, 20260101 + (i * 37) % 300, (i * 13) % 1000);
    rlSortNewestFirst(l);
    CHECK_EQ(l.size, 1000);
    int count = 0;
    bool ordered = true;
    for (Reservation* r = l.head; r; r = r->next) {
        count++;
        if (r->next && (r->day < r->next->day || (r->day == r->next->day && r->reservedAt < r->next->reservedAt)))
            ordered = false;
    }
    CHECK_EQ(count, 1000);
    CHECK(ordered);
    rlClear(l);
}
