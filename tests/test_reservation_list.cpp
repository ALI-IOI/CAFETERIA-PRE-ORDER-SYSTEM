#include "testing.h"
#include "ds/reservation_list.h"

static const int DAY = 20261023;

TEST(list_append_sets_head_and_tail) {
    ReservationList list;
    rlInit(list);
    CHECK(list.head == nullptr);
    CHECK(list.tail == nullptr);
    Reservation* a = rlAppend(list, "a@uni.example", 0, 1, 1000, DAY);
    CHECK(list.head == a);
    CHECK(list.tail == a);
    Reservation* b = rlAppend(list, "b@uni.example", 1, 2, 1001, DAY);
    CHECK(list.head == a);
    CHECK(list.tail == b);
    CHECK(a->next == b);
    CHECK(b->next == nullptr);
    CHECK_EQ(list.size, 2);
    CHECK_EQ(b->status, ResStatus::Reserved);
    CHECK_EQ(b->quantity, 2);
    CHECK_EQ(b->user, std::string("b@uni.example"));
    rlClear(list);
}

TEST(list_ids_increase_from_nextId) {
    ReservationList list;
    rlInit(list);
    CHECK_EQ(list.nextId, 1);
    list.nextId = 42;
    Reservation* a = rlAppend(list, "a@uni.example", 0, 1, 1000, DAY);
    Reservation* b = rlAppend(list, "a@uni.example", 0, 1, 1000, DAY);
    CHECK_EQ(a->id, 42);
    CHECK_EQ(b->id, 43);
    CHECK_EQ(list.nextId, 44);
    CHECK_EQ(resCode(42), std::string("R-0042"));
    CHECK_EQ(resCode(12345), std::string("R-12345"));
    rlClear(list);
}

TEST(list_find_missing_returns_null) {
    ReservationList list;
    rlInit(list);
    CHECK(rlFindById(list, 1) == nullptr);
    Reservation* a = rlAppend(list, "a@uni.example", 0, 1, 1000, DAY);
    CHECK(rlFindById(list, a->id) == a);
    CHECK(rlFindById(list, 999) == nullptr);
    rlClear(list);
}

TEST(list_delete_head_middle_tail_updates_tail_pointer) {
    ReservationList list;
    rlInit(list);
    for (int i = 0; i < 5; i++) rlAppend(list, "u@uni.example", 0, 1, 1000 + i, DAY);
    // ids 1..5
    CHECK(rlDelete(list, 3));        // middle
    CHECK(rlDelete(list, 1));        // head
    CHECK_EQ(list.head->id, 2);
    CHECK(rlDelete(list, 5));        // tail
    CHECK_EQ(list.tail->id, 4);
    CHECK(list.tail->next == nullptr);
    CHECK_EQ(list.size, 2);
    CHECK(!rlDelete(list, 3));       // already gone
    Reservation* n = rlAppend(list, "u@uni.example", 0, 1, 2000, DAY);
    CHECK(list.tail == n);           // tail still correct after deletes
    CHECK_EQ(list.head->next->next, n);
    rlClear(list);
}

TEST(list_delete_only_node_empties_list) {
    ReservationList list;
    rlInit(list);
    Reservation* a = rlAppend(list, "a@uni.example", 0, 1, 1000, DAY);
    CHECK(rlDelete(list, a->id));
    CHECK(list.head == nullptr);
    CHECK(list.tail == nullptr);
    CHECK_EQ(list.size, 0);
}

TEST(list_clear_frees_all) {
    ReservationList list;
    rlInit(list);
    for (int i = 0; i < 10; i++) rlAppend(list, "u@uni.example", 0, 1, 1000, DAY);
    rlClear(list);
    CHECK(list.head == nullptr);
    CHECK(list.tail == nullptr);
    CHECK_EQ(list.size, 0);
}

TEST(status_names_match_proposal) {
    CHECK_EQ(std::string(statusName(ResStatus::Reserved)), std::string("Reserved"));
    CHECK_EQ(std::string(statusName(ResStatus::Ready)), std::string("Ready"));
    CHECK_EQ(std::string(statusName(ResStatus::Collected)), std::string("Collected"));
    CHECK_EQ(std::string(statusName(ResStatus::Cancelled)), std::string("Cancelled"));
    CHECK_EQ(std::string(statusName(ResStatus::Expired)), std::string("Expired"));
}
