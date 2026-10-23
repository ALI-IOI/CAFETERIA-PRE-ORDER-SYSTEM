// Waitlist for sold-out fresh items (suggested addition #1). One circular
// WaitQueue per menu slot; promotion turns the front entry into a real
// reservation through the normal reserve() checks.
#include "core/cafeteria.h"

int waitlistActiveCount(const Cafeteria& caf, int itemIndex) {
    if (itemIndex < 0 || itemIndex >= MAX_MENU_ITEMS) return 0;
    const WaitQueue& q = caf.waitlists[itemIndex];
    int n = 0;
    for (int i = 0; i < q.count; i++) {
        if (wqAt(q, i)->active) n++;
    }
    return n;
}

int waitlistPosition(const Cafeteria& caf, const std::string& user, int itemIndex) {
    if (itemIndex < 0 || itemIndex >= MAX_MENU_ITEMS) return 0;
    const WaitQueue& q = caf.waitlists[itemIndex];
    int place = 0;
    for (int i = 0; i < q.count; i++) {
        const WaitEntry* e = wqAt(q, i);
        if (!e->active) continue;
        place++;
        if (e->user == user) return place;
    }
    return 0;
}

Result joinWaitlist(Cafeteria& caf, const std::string& user, int itemIndex, int quantity, int& position) {
    position = 0;
    cafEnsureToday(caf);
    MenuItem* item = menuAt(caf.menu, itemIndex);
    if (item == nullptr) return Result::BadItem;
    if (item->kind != ItemKind::Fresh) return Result::NotFresh;
    if (!item->available) return Result::NotAvailable;
    if (quantity < 1 || quantity > caf.settings.perUserLimit) return Result::BadQuantity;
    if (!beforeMinuteOfDay(caf, caf.settings.cutoffMinute)) return Result::CutoffPassed;
    if (quantity <= menuRemaining(*item)) return Result::NotSoldOut;
    if (userPortionsToday(caf, user, itemIndex) + quantity > caf.settings.perUserLimit) return Result::UserLimit;
    if (waitlistPosition(caf, user, itemIndex) != 0) return Result::AlreadyWaiting;
    if (wqIsFull(caf.waitlists[itemIndex])) {
        // Compact: people who left still occupy slots until they reach the
        // front. Rotate once through the queue keeping only active entries.
        WaitQueue& q = caf.waitlists[itemIndex];
        int n = q.count;
        WaitEntry e;
        for (int i = 0; i < n; i++) {
            wqDequeue(q, e);
            if (e.active) wqEnqueue(q, e);
        }
        if (wqIsFull(q)) return Result::WaitlistFull;
    }

    WaitEntry e;
    e.user = user;
    e.quantity = quantity;
    e.joinedAt = caf.clock->now();
    e.active = true;
    wqEnqueue(caf.waitlists[itemIndex], e);
    position = waitlistActiveCount(caf, itemIndex);
    return Result::Ok;
}

Result leaveWaitlist(Cafeteria& caf, const std::string& user, int itemIndex) {
    if (itemIndex < 0 || itemIndex >= MAX_MENU_ITEMS) return Result::BadItem;
    WaitQueue& q = caf.waitlists[itemIndex];
    for (int i = 0; i < q.count; i++) {
        WaitEntry* e = wqAt(q, i);
        if (e->active && e->user == user) {
            e->active = false;  // stays in place; skipped when it reaches the front
            return Result::Ok;
        }
    }
    return Result::NotWaiting;
}

int promoteWaitlist(Cafeteria& caf, int itemIndex, NotificationList& out) {
    MenuItem* item = menuAt(caf.menu, itemIndex);
    if (item == nullptr || item->kind != ItemKind::Fresh) return 0;
    if (!item->available) return 0;  // paused: people keep their place until it is back
    if (!beforeMinuteOfDay(caf, caf.settings.cutoffMinute)) return 0;

    WaitQueue& q = caf.waitlists[itemIndex];
    int made = 0;
    WaitEntry front;
    while (!wqIsEmpty(q)) {
        WaitEntry* e = wqPeek(q);
        if (!e->active) {  // left the waitlist
            wqDequeue(q, front);
            continue;
        }
        if (userPortionsToday(caf, e->user, itemIndex) + e->quantity > caf.settings.perUserLimit) {
            wqDequeue(q, front);  // reserved enough meanwhile: skip, next person
            continue;
        }
        if (e->quantity > menuRemaining(*item)) break;  // strict FIFO: nobody jumps ahead
        Result r = reserveWithNote(caf, e->user, itemIndex, e->quantity, out, nullptr,
                                   "Good news: a portion became free and you were first on the waitlist.\n");
        if (r == Result::QueueFull) break;  // try again when the cooking queue has room
        wqDequeue(q, front);
        if (r == Result::Ok) made++;
    }
    return made;
}

void closeWaitlistsAtCutoff(Cafeteria& caf, NotificationList& out) {
    if (beforeMinuteOfDay(caf, caf.settings.cutoffMinute)) return;
    for (int i = 0; i < caf.menu.count; i++) {
        WaitQueue& q = caf.waitlists[i];
        WaitEntry e;
        while (wqDequeue(q, e)) {
            if (!e.active) continue;
            const std::string& name = caf.menu.items[i].name;
            nlAppend(out, NotifType::WaitlistClosed, e.user, "No " + name + " portion became free today",
                     "Sorry - no " + name + " portion became free before the " +
                         formatMinuteOfDay(caf.settings.cutoffMinute) +
                         " cutoff, so the waitlist has closed for today.",
                     0);
        }
    }
}
