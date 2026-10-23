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
    caf.settings = settingsDefaults();
    caf.clock = clock;
    caf.currentDay = dayKeyOf(clock->now());
    caf.cookingCapacity = cookingCapacity;
}

void cafFree(Cafeteria& caf) {
    for (int i = 0; i < MAX_MENU_ITEMS; i++) qFree(caf.cooking[i]);
    qFree(caf.reminders);
    rlClear(caf.reservations);
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
