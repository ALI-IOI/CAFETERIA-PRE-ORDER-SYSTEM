#include "testing.h"
#include "cafe_fixture.h"

static const char* STAFF = "staff1";

TEST(add_item_appends_at_next_index) {
    CafeFixture f;  // 4 items
    int idx = -1;
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Fresh food", "Biryani", ItemKind::Fresh, 150, 20, idx), Result::Ok);
    CHECK_EQ(idx, 4);
    const MenuItem& m = f.caf.menu.items[4];
    CHECK_EQ(m.name, std::string("Biryani"));
    CHECK_EQ(m.priceTaka, 150);
    CHECK_EQ(m.dailyLimit, 20);
    CHECK(m.available);
    CHECK_EQ(m.imageFile, std::string(""));
    CHECK(f.reserveOk(ALICE, 4, 1) != nullptr);  // new fresh item is reservable
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Chips", "Pringles", ItemKind::Packaged, 90, 12, idx), Result::Ok);
    CHECK_EQ(f.caf.menu.items[idx].stock, 12);
}

TEST(add_item_refused_when_full) {
    CafeFixture f;
    int idx;
    for (int i = f.caf.menu.count; i < MAX_MENU_ITEMS; i++) {
        CHECK_EQ(addMenuItem(f.caf, STAFF, "Chips", "Item " + std::to_string(i), ItemKind::Packaged, 10, 1, idx),
                 Result::Ok);
    }
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Chips", "One more", ItemKind::Packaged, 10, 1, idx), Result::MenuFull);
    CHECK_EQ(f.caf.menu.count, MAX_MENU_ITEMS);
}

TEST(add_item_duplicate_name_refused) {
    CafeFixture f;
    int idx;
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Fresh food", "Tehari", ItemKind::Fresh, 120, 5, idx), Result::InvalidValue);
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Fresh food", "Haleem", ItemKind::Fresh, 10000, 5, idx), Result::InvalidValue);
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Fresh food", "Haleem", ItemKind::Fresh, 100, 101, idx), Result::InvalidValue);
    CHECK_EQ(f.caf.menu.count, 4);
}

TEST(item_name_validation) {
    CafeFixture f;
    int idx;
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Fresh food", "Ta|hari", ItemKind::Fresh, 100, 5, idx), Result::InvalidValue);
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Fresh food", "Line\nbreak", ItemKind::Fresh, 100, 5, idx), Result::InvalidValue);
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Fresh food", std::string(61, 'x'), ItemKind::Fresh, 100, 5, idx),
             Result::InvalidValue);
    CHECK_EQ(addMenuItem(f.caf, STAFF, "", "Haleem", ItemKind::Fresh, 100, 5, idx), Result::InvalidValue);
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Fresh food", "<b>Haleem</b>", ItemKind::Fresh, 100, 5, idx), Result::Ok);
}

// The category starts the menu.txt line, and lines starting with '#' are comments:
// such an item would vanish on restart and shift every later item's index.
TEST(category_starting_with_hash_refused) {
    CafeFixture f;
    int idx;
    CHECK_EQ(addMenuItem(f.caf, STAFF, "#1 Specials", "Dal", ItemKind::Fresh, 50, 5, idx), Result::InvalidValue);
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Specials #1", "Dal", ItemKind::Fresh, 50, 5, idx), Result::Ok);
}

TEST(set_price_and_undo) {
    CafeFixture f;
    bool rec;
    CHECK_EQ(setPrice(f.caf, STAFF, TEHARI, 120, rec), Result::Ok);
    CHECK(rec);
    CHECK_EQ(f.caf.menu.items[TEHARI].priceTaka, 120);
    CHECK_EQ(stPeek(f.caf.history)->description, std::string("Set Tehari price 0 -> 120"));
    CHECK_EQ(setPrice(f.caf, STAFF, TEHARI, -5, rec), Result::InvalidValue);
    StaffAction u;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, u), Result::Ok);
    CHECK_EQ(f.caf.menu.items[TEHARI].priceTaka, 0);
}

TEST(set_available_off_refuses_reserve_and_waitlist) {
    CafeFixture f;
    bool rec;
    CHECK_EQ(setAvailable(f.caf, STAFF, TEHARI, false, f.out, rec), Result::Ok);
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, ALICE, TEHARI, 1, f.out, &r), Result::NotAvailable);
    f.caf.menu.items[FRIED_RICE].dailyLimit = 0;  // sold out
    setAvailable(f.caf, STAFF, FRIED_RICE, false, f.out, rec);
    int pos;
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::NotAvailable);
}

TEST(existing_reservation_survives_unavailable) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    bool rec;
    setAvailable(f.caf, STAFF, TEHARI, false, f.out, rec);
    ReadyReport rep;
    CHECK_EQ(markReady(f.caf, STAFF, TEHARI, 0, f.out, rep), Result::Ok);
    CHECK_EQ(a->status, ResStatus::Ready);
}

TEST(waitlist_pauses_while_unavailable) {
    CafeFixture f;
    f.caf.menu.items[FRIED_RICE].dailyLimit = 1;
    Reservation* a = f.reserveOk(ALICE, FRIED_RICE, 1);
    int pos;
    joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos);
    bool rec;
    setAvailable(f.caf, STAFF, FRIED_RICE, false, f.out, rec);
    cancelReservation(f.caf, ALICE, a->id, f.out);
    CHECK_EQ(userPortionsToday(f.caf, BOB, FRIED_RICE), 0);  // paused
    CHECK_EQ(waitlistPosition(f.caf, BOB, FRIED_RICE), 1);   // keeps his place
    setAvailable(f.caf, STAFF, FRIED_RICE, true, f.out, rec);
    CHECK_EQ(userPortionsToday(f.caf, BOB, FRIED_RICE), 1);  // promoted when it comes back
}

TEST(undo_set_available) {
    CafeFixture f;
    bool rec;
    setAvailable(f.caf, STAFF, TEHARI, false, f.out, rec);
    CHECK_EQ(stPeek(f.caf.history)->description, std::string("Made Tehari unavailable"));
    StaffAction u;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, u), Result::Ok);
    CHECK(f.caf.menu.items[TEHARI].available);
}

TEST(set_item_image_not_undoable) {
    CafeFixture f;
    CHECK_EQ(setItemImage(f.caf, STAFF, TEHARI, "item-2-00ff00ff00ff00ff.png"), Result::Ok);
    CHECK_EQ(f.caf.menu.items[TEHARI].imageFile, std::string("item-2-00ff00ff00ff00ff.png"));
    CHECK(stIsEmpty(f.caf.history));
    CHECK_EQ(setItemImage(f.caf, STAFF, 99, "x.png"), Result::BadItem);
}

// ---- removing items: retired, not erased (reservations refer to the index) ----

TEST(remove_item_hides_it_and_refuses_reserve) {
    CafeFixture f;
    bool rec = false;
    CHECK_EQ(removeMenuItem(f.caf, STAFF, KHICHURI, rec), Result::Ok);
    CHECK(rec);
    CHECK(f.caf.menu.items[KHICHURI].removed);
    CHECK(!f.caf.menu.items[KHICHURI].available);
    CHECK_EQ(f.caf.menu.count, 4);  // keeps its place: later indices do not move
    CHECK_EQ(stPeek(f.caf.history)->description, std::string("Removed Khichuri"));
    Reservation* r = nullptr;
    CHECK_EQ(reserve(f.caf, ALICE, KHICHURI, 1, f.out, &r), Result::NotAvailable);
}

TEST(remove_item_refused_while_reserved_or_ready) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, TEHARI, 1);
    bool rec = false;
    CHECK_EQ(removeMenuItem(f.caf, STAFF, TEHARI, rec), Result::ItemInUse);
    CHECK(!rec);
    ReadyReport rep;
    markReady(f.caf, STAFF, TEHARI, 0, f.out, rep);
    CHECK_EQ(removeMenuItem(f.caf, STAFF, TEHARI, rec), Result::ItemInUse);  // ready, not collected yet
    CHECK_EQ(markCollected(f.caf, STAFF, a->id, rec), Result::Ok);
    CHECK_EQ(removeMenuItem(f.caf, STAFF, TEHARI, rec), Result::Ok);
    CHECK(f.caf.menu.items[TEHARI].removed);
}

TEST(remove_item_allowed_after_cancel_refused_with_waitlist) {
    CafeFixture f;
    Reservation* a = f.reserveOk(ALICE, KHICHURI, 1);
    cancelReservation(f.caf, ALICE, a->id, f.out);
    bool rec;
    CHECK_EQ(removeMenuItem(f.caf, STAFF, KHICHURI, rec), Result::Ok);  // a cancellation does not hold it
    f.caf.menu.items[FRIED_RICE].dailyLimit = 0;  // sold out
    int pos;
    CHECK_EQ(joinWaitlist(f.caf, BOB, FRIED_RICE, 1, pos), Result::Ok);
    CHECK_EQ(removeMenuItem(f.caf, STAFF, FRIED_RICE, rec), Result::ItemInUse);
}

TEST(remove_item_bad_or_already_removed) {
    CafeFixture f;
    bool rec;
    CHECK_EQ(removeMenuItem(f.caf, STAFF, 99, rec), Result::BadItem);
    CHECK_EQ(removeMenuItem(f.caf, STAFF, LAYS, rec), Result::Ok);
    CHECK_EQ(removeMenuItem(f.caf, STAFF, LAYS, rec), Result::BadItem);
}

TEST(undo_remove_item_restores_it) {
    CafeFixture f;
    bool rec;
    removeMenuItem(f.caf, STAFF, KHICHURI, rec);
    StaffAction u;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, u), Result::Ok);
    CHECK(!f.caf.menu.items[KHICHURI].removed);
    CHECK(f.caf.menu.items[KHICHURI].available);
    CHECK(f.reserveOk(ALICE, KHICHURI, 1) != nullptr);
    // an item that was switched off before it was removed comes back switched off
    setAvailable(f.caf, STAFF, TEHARI, false, f.out, rec);
    removeMenuItem(f.caf, STAFF, TEHARI, rec);
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, u), Result::Ok);
    CHECK(!f.caf.menu.items[TEHARI].removed);
    CHECK(!f.caf.menu.items[TEHARI].available);
}

TEST(removed_item_name_can_be_reused) {
    CafeFixture f;
    bool rec;
    removeMenuItem(f.caf, STAFF, KHICHURI, rec);
    int idx = -1;
    CHECK_EQ(addMenuItem(f.caf, STAFF, "Fresh food", "Khichuri", ItemKind::Fresh, 90, 10, idx), Result::Ok);
    CHECK_EQ(idx, 4);
    // undoing the old removal now would give two items the same name
    StaffAction u;
    CHECK_EQ(undoLast(f.caf, STAFF, f.out, u), Result::UndoBlocked);
    CHECK(f.caf.menu.items[KHICHURI].removed);
}

TEST(removed_item_cannot_be_switched_on_or_repriced) {
    CafeFixture f;
    bool rec;
    removeMenuItem(f.caf, STAFF, TEHARI, rec);
    CHECK_EQ(setAvailable(f.caf, STAFF, TEHARI, true, f.out, rec), Result::BadItem);
    CHECK_EQ(setPrice(f.caf, STAFF, TEHARI, 50, rec), Result::BadItem);
    CHECK(!f.caf.menu.items[TEHARI].available);
}
