// Shared test fixture: a cafeteria with the proposal's three fresh items and
// one packaged item, on a fake clock set to 2026-10-23 09:00.
#pragma once
#include "core/cafeteria.h"

const int KHICHURI = 0, FRIED_RICE = 1, TEHARI = 2, LAYS = 3;
const char* const ALICE = "alice@uni.example";
const char* const BOB = "bob@uni.example";
const char* const CAROL = "carol@uni.example";

struct CafeFixture {
    FakeClock clock;
    Cafeteria caf;
    NotificationList out;

    explicit CafeFixture(int cookingCapacity = 100) {
        clock.set(2026, 10, 23, 9, 0, 0);
        cafInit(caf, &clock, cookingCapacity);
        addItem("Fresh food", "Khichuri", ItemKind::Fresh, 0, 12);
        addItem("Fresh food", "Fried Rice", ItemKind::Fresh, 0, 8);
        addItem("Fresh food", "Tehari", ItemKind::Fresh, 0, 10);
        addItem("Chips", "Lays", ItemKind::Packaged, 20, 0);
        nlInit(out);
    }
    ~CafeFixture() {
        nlClear(out);
        cafFree(caf);
    }
    void addItem(const char* cat, const char* name, ItemKind kind, int stock, int limit) {
        MenuItem m;
        m.category = cat;
        m.name = name;
        m.kind = kind;
        m.stock = stock;
        m.dailyLimit = limit;
        m.reservedToday = 0;
        menuAdd(caf.menu, m);
    }
    Reservation* reserveOk(const char* user, int item, int qty) {
        Reservation* r = nullptr;
        Result res = reserve(caf, user, item, qty, out, &r);
        return res == Result::Ok ? r : nullptr;
    }
    int countType(NotifType t) const {
        int n = 0;
        for (Notification* x = out.head; x; x = x->next)
            if (x->type == t) n++;
        return n;
    }
};
