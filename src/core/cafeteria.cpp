#include "core/cafeteria.h"

const char* resultMessage(Result r) {
    switch (r) {
        case Result::Ok: return "OK";
        case Result::BadItem: return "That item does not exist.";
        case Result::NotFresh: return "Packaged items are view-only and cannot be reserved.";
        case Result::BadQuantity: return "Quantity must be between 1 and the per-person limit.";
        case Result::CutoffPassed: return "The reservation cutoff time has passed for today.";
        case Result::SoldOut: return "Not enough portions left for that quantity.";
        case Result::UserLimit: return "You have reached the per-person limit for this item today.";
        case Result::QueueFull: return "Too many reservations for this item today.";
        case Result::NotFound: return "Reservation not found.";
        case Result::NotOwner: return "Reservation not found.";
        case Result::NotCancellable: return "Only reservations with status Reserved can be cancelled.";
        case Result::DeadlinePassed: return "The cancellation deadline has passed.";
        case Result::InvalidValue: return "That value is not allowed.";
        case Result::NothingToUndo: return "There is no staff action to undo.";
        case Result::UndoBlocked: return "The last action cannot be undone because later changes depend on it.";
        case Result::NotReady: return "Only reservations with status Ready can be marked collected.";
        case Result::WrongKind: return "That action does not apply to this kind of item.";
    }
    return "Unknown error.";
}

void cafInit(Cafeteria& caf, Clock* clock, int cookingCapacity) {
    menuInit(caf.menu);
    rlInit(caf.reservations);
    for (int i = 0; i < MAX_MENU_ITEMS; i++) qInit(caf.cooking[i], cookingCapacity);
    // Every reservation is in the reminder queue at most once, so this can hold all of them.
    qInit(caf.reminders, cookingCapacity * MAX_MENU_ITEMS);
    stInit(caf.history);
    rlInit(caf.archive);
    caf.settings = settingsDefaults();
    caf.clock = clock;
    caf.currentDay = dayKeyOf(clock->now());
    caf.cookingCapacity = cookingCapacity;
}

void cafFree(Cafeteria& caf) {
    for (int i = 0; i < MAX_MENU_ITEMS; i++) qFree(caf.cooking[i]);
    qFree(caf.reminders);
    rlClear(caf.reservations);
    rlClear(caf.archive);
}

bool beforeMinuteOfDay(const Cafeteria& caf, int minuteOfDay) {
    return secondOfDay(caf.clock->now()) < minuteOfDay * 60;
}

int userPortionsToday(const Cafeteria& caf, const std::string& user, int itemIndex) {
    int today = dayKeyOf(caf.clock->now());
    int total = 0;
    for (Reservation* r = caf.reservations.head; r != nullptr; r = r->next) {
        if (r->user == user && r->itemIndex == itemIndex && r->day == today &&
            r->status != ResStatus::Cancelled) {
            total += r->quantity;
        }
    }
    return total;
}

Result reserve(Cafeteria& caf, const std::string& user, int itemIndex, int quantity,
               NotificationList& out, Reservation** created) {
    if (created) *created = nullptr;
    MenuItem* item = menuAt(caf.menu, itemIndex);
    if (item == nullptr) return Result::BadItem;
    if (item->kind != ItemKind::Fresh) return Result::NotFresh;
    if (quantity < 1 || quantity > caf.settings.perUserLimit) return Result::BadQuantity;
    if (!beforeMinuteOfDay(caf, caf.settings.cutoffMinute)) return Result::CutoffPassed;
    if (quantity > menuRemaining(*item)) return Result::SoldOut;
    if (userPortionsToday(caf, user, itemIndex) + quantity > caf.settings.perUserLimit)
        return Result::UserLimit;
    if (qIsFull(caf.cooking[itemIndex])) return Result::QueueFull;

    time_t now = caf.clock->now();
    Reservation* r = rlAppend(caf.reservations, user, itemIndex, quantity, now, dayKeyOf(now));
    item->reservedToday += quantity;
    qEnqueue(caf.cooking[itemIndex], r);

    std::string code = resCode(r->id);
    nlAppend(out, NotifType::Confirmation, user, "Reservation " + code + " confirmed",
             "Reservation " + code + " confirmed: " + item->name + " x " +
                 std::to_string(quantity) + ".\n" +
                 "We will email you when it is ready for collection.\n" +
                 "You can cancel until " + formatMinuteOfDay(caf.settings.cancelDeadlineMinute) +
                 " today.",
             r->id);
    if (created) *created = r;
    return Result::Ok;
}

Result cancelReservation(Cafeteria& caf, const std::string& user, int reservationId) {
    Reservation* r = rlFindById(caf.reservations, reservationId);
    if (r == nullptr) return Result::NotFound;
    if (r->user != user) return Result::NotOwner;
    if (r->status != ResStatus::Reserved) return Result::NotCancellable;
    if (r->day != dayKeyOf(caf.clock->now()) ||
        !beforeMinuteOfDay(caf, caf.settings.cancelDeadlineMinute))
        return Result::DeadlinePassed;

    r->status = ResStatus::Cancelled;  // stays in the cooking queue; skipped at the front
    MenuItem* item = menuAt(caf.menu, r->itemIndex);
    if (item) item->reservedToday -= r->quantity;
    return Result::Ok;
}

int listUserReservations(const Cafeteria& caf, const std::string& user,
                         Reservation** outArr, int cap) {
    int n = 0;
    for (Reservation* r = caf.reservations.head; r != nullptr && n < cap; r = r->next) {
        if (r->user == user) outArr[n++] = r;
    }
    return n;
}

std::string durationText(int minutes) {
    if (minutes < 0) minutes = 0;
    int h = minutes / 60, m = minutes % 60;
    std::string text;
    if (h > 0) text = std::to_string(h) + (h == 1 ? " hour" : " hours");
    if (m > 0 || h == 0) {
        if (!text.empty()) text += " ";
        text += std::to_string(m) + (m == 1 ? " minute" : " minutes");
    }
    return text;
}

static const std::string& itemName(const Cafeteria& caf, const Reservation* r) {
    static const std::string unknown = "food";
    const MenuItem* item = menuAt(caf.menu, r->itemIndex);
    return item ? item->name : unknown;
}

Result markReady(Cafeteria& caf, const std::string& staff, int itemIndex, int portions,
                 NotificationList& out, ReadyReport& report) {
    (void)staff;
    report.madeReady = 0;
    report.skipped = 0;
    report.leftWaiting = 0;
    MenuItem* item = menuAt(caf.menu, itemIndex);
    if (item == nullptr) return Result::BadItem;
    if (item->kind != ItemKind::Fresh) return Result::NotFresh;
    if (portions < 0) return Result::InvalidValue;

    ResQueue& q = caf.cooking[itemIndex];
    time_t now = caf.clock->now();
    int portionsLeft = portions;
    while (!qIsEmpty(q)) {
        Reservation* r = qPeek(q);
        if (r->status != ResStatus::Reserved) {  // cancelled: skip it
            qDequeue(q);
            report.skipped++;
            continue;
        }
        if (portions > 0 && r->quantity > portionsLeft) break;
        qDequeue(q);
        portionsLeft -= r->quantity;
        r->status = ResStatus::Ready;
        r->readyAt = now;
        r->lastNoticeAt = now;
        qEnqueue(caf.reminders, r);  // capacity covers every reservation (see cafInit)
        report.madeReady++;

        std::string code = resCode(r->id);
        nlAppend(out, NotifType::Ready, r->user, "Your " + item->name + " (" + code + ") is ready",
                 "Your " + item->name + " (" + code + ") is ready for collection.\n" +
                     "It stays fresh for about " + durationText(caf.settings.freshnessMinutes) +
                     " (until " + formatClock(now + (time_t)caf.settings.freshnessMinutes * 60) + ").",
                 r->id);
    }
    report.leftWaiting = q.count;
    return Result::Ok;
}

Result markCollected(Cafeteria& caf, const std::string& staff, int reservationId,
                     bool& undoRecorded) {
    undoRecorded = false;
    Reservation* r = rlFindById(caf.reservations, reservationId);
    if (r == nullptr) return Result::NotFound;
    if (r->status != ResStatus::Ready) return Result::NotReady;
    r->status = ResStatus::Collected;  // leaves the reminder queue when it reaches the front

    StaffAction a;
    a.type = ActionType::MarkCollected;
    a.staff = staff;
    a.itemIndex = r->itemIndex;
    a.reservationId = r->id;
    a.oldValue = (int)ResStatus::Ready;
    a.newValue = (int)ResStatus::Collected;
    a.at = caf.clock->now();
    a.description = "Marked " + resCode(r->id) + " (" + itemName(caf, r) + ") collected";
    undoRecorded = stPush(caf.history, a);
    return Result::Ok;
}

// D9: a new day. Uncooked reservations expire, counters reset, cooking queues
// empty, and finished reservations from earlier days move to the archive list.
static void rollover(Cafeteria& caf, int newDay) {
    for (Reservation* r = caf.reservations.head; r != nullptr; r = r->next) {
        if (r->day < newDay && r->status == ResStatus::Reserved) r->status = ResStatus::Expired;
    }
    for (int i = 0; i < caf.menu.count; i++) {
        if (caf.menu.items[i].kind == ItemKind::Fresh) caf.menu.items[i].reservedToday = 0;
        qClear(caf.cooking[i]);
    }
    // Drop non-Ready entries from the reminder queue (keeping order) so no
    // queue points at a node we are about to move.
    int n = caf.reminders.count;
    for (int i = 0; i < n; i++) {
        Reservation* r = qDequeue(caf.reminders);
        if (r->status == ResStatus::Ready) qEnqueue(caf.reminders, r);
    }
    Reservation* r = caf.reservations.head;
    while (r != nullptr) {
        Reservation* next = r->next;
        if (r->day < newDay && r->status != ResStatus::Ready) {
            rlAppendNode(caf.archive, rlUnlink(caf.reservations, r->id));
        }
        r = next;
    }
    caf.currentDay = newDay;
}

void timerTick(Cafeteria& caf, NotificationList& out) {
    time_t now = caf.clock->now();
    int today = dayKeyOf(now);
    if (today != caf.currentDay) rollover(caf, today);

    const time_t interval = (time_t)caf.settings.reminderIntervalMinutes * 60;
    const time_t window = (time_t)caf.settings.freshnessMinutes * 60;
    // Every reminder is due one interval after the previous notice, so the
    // front of the queue is always the next one due (proposal section 8).
    while (!qIsEmpty(caf.reminders)) {
        Reservation* r = qPeek(caf.reminders);
        if (r->status != ResStatus::Ready) {  // collected: leaves the queue
            qDequeue(caf.reminders);
            continue;
        }
        std::string code = resCode(r->id);
        const std::string& name = itemName(caf, r);
        time_t expiresAt = r->readyAt + window;
        if (now >= expiresAt) {
            qDequeue(caf.reminders);
            r->status = ResStatus::Expired;
            nlAppend(out, NotifType::Expired, r->user, "Your " + name + " (" + code + ") has expired",
                     "Your " + name + " (" + code + ") was not collected within its freshness window " +
                         "and has expired. Please reserve again next time.",
                     r->id);
            continue;
        }
        time_t due = r->lastNoticeAt + interval;
        if (now < due) break;  // the front is not due, so nothing behind it is
        qDequeue(caf.reminders);
        // Keep the schedule anchored to the ready time unless we fell a whole
        // interval behind (e.g. the server was down).
        r->lastNoticeAt = (now - due < interval) ? due : now;
        int minutesLeft = (int)((expiresAt - now) / 60);
        std::string left = minutesLeft >= 60
                               ? std::to_string(minutesLeft / 60) + (minutesLeft / 60 == 1 ? " hour" : " hours")
                               : durationText(minutesLeft);
        nlAppend(out, NotifType::Reminder, r->user, "Reminder: your " + name + " (" + code + ") is ready",
                 "Reminder: your " + name + " (" + code + ") is ready. About " + left +
                     " of freshness left.",
                 r->id);
        qEnqueue(caf.reminders, r);
    }
}
