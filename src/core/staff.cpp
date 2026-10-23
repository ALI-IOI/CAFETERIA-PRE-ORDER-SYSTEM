// Staff portal operations and the undo stack (proposal sections 5 and 7).
#include "core/cafeteria.h"
#include "auth/validation.h"

static StaffAction newAction(Cafeteria& caf, ActionType type, const std::string& staff) {
    StaffAction a;
    a.type = type;
    a.staff = staff;
    a.itemIndex = -1;
    a.reservationId = 0;
    a.oldValue = 0;
    a.newValue = 0;
    a.at = caf.clock->now();
    return a;
}

static bool isTimeSetting(const std::string& key) {
    return key == "cutoff" || key == "cancel_deadline";
}

static std::string settingValueText(const std::string& key, int value) {
    return isTimeSetting(key) ? formatMinuteOfDay(value) : std::to_string(value);
}

Result setStock(Cafeteria& caf, const std::string& staff, int itemIndex, int value,
                bool& undoRecorded) {
    undoRecorded = false;
    MenuItem* item = menuAt(caf.menu, itemIndex);
    if (item == nullptr) return Result::BadItem;
    if (item->kind != ItemKind::Packaged) return Result::WrongKind;
    if (value < 0 || value > MAX_STOCK) return Result::InvalidValue;

    StaffAction a = newAction(caf, ActionType::SetStock, staff);
    a.itemIndex = itemIndex;
    a.oldValue = item->stock;
    a.newValue = value;
    a.description = "Set " + item->name + " stock " + std::to_string(a.oldValue) + " -> " +
                    std::to_string(value);
    item->stock = value;
    undoRecorded = stPush(caf.history, a);
    return Result::Ok;
}

Result setDailyLimit(Cafeteria& caf, const std::string& staff, int itemIndex, int value,
                     NotificationList& out, bool& undoRecorded) {
    undoRecorded = false;
    cafEnsureToday(caf);  // the limit is checked against today's reserved count
    MenuItem* item = menuAt(caf.menu, itemIndex);
    if (item == nullptr) return Result::BadItem;
    if (item->kind != ItemKind::Fresh) return Result::WrongKind;
    if (value < item->reservedToday || value > caf.cookingCapacity) return Result::InvalidValue;

    StaffAction a = newAction(caf, ActionType::SetDailyLimit, staff);
    a.itemIndex = itemIndex;
    a.oldValue = item->dailyLimit;
    a.newValue = value;
    a.description = "Set " + item->name + " daily limit " + std::to_string(a.oldValue) + " -> " +
                    std::to_string(value);
    item->dailyLimit = value;
    undoRecorded = stPush(caf.history, a);
    promoteWaitlist(caf, itemIndex, out);  // a raise frees portions for people waiting
    return Result::Ok;
}

Result changeSetting(Cafeteria& caf, const std::string& staff, const std::string& key,
                     int value, bool& undoRecorded) {
    undoRecorded = false;
    int oldValue;
    if (!settingsGet(caf.settings, key, oldValue)) return Result::InvalidValue;
    if (!settingsSet(caf.settings, key, value)) return Result::InvalidValue;

    StaffAction a = newAction(caf, ActionType::SetSetting, staff);
    a.settingKey = key;
    a.oldValue = oldValue;
    a.newValue = value;
    a.description = "Changed " + key + " " + settingValueText(key, oldValue) + " -> " +
                    settingValueText(key, value);
    undoRecorded = stPush(caf.history, a);
    return Result::Ok;
}

static bool inQueue(const ResQueue& q, const Reservation* r) {
    for (int i = 0; i < q.count; i++) {
        if (qAt(q, i) == r) return true;
    }
    return false;
}

// Restores one action. Returns false when it can no longer be undone.
static bool restore(Cafeteria& caf, const StaffAction& a) {
    switch (a.type) {
        case ActionType::SetStock: {
            MenuItem* item = menuAt(caf.menu, a.itemIndex);
            if (item == nullptr) return false;
            item->stock = a.oldValue;
            return true;
        }
        case ActionType::SetDailyLimit: {
            MenuItem* item = menuAt(caf.menu, a.itemIndex);
            if (item == nullptr || a.oldValue < item->reservedToday) return false;
            item->dailyLimit = a.oldValue;
            return true;
        }
        case ActionType::SetSetting:
            return settingsSet(caf.settings, a.settingKey, a.oldValue);
        case ActionType::SetPrice: {
            MenuItem* item = menuAt(caf.menu, a.itemIndex);
            if (item == nullptr) return false;
            item->priceTaka = a.oldValue;
            return true;
        }
        case ActionType::SetAvailable: {
            MenuItem* item = menuAt(caf.menu, a.itemIndex);
            if (item == nullptr) return false;
            item->available = a.oldValue != 0;
            return true;
        }
        case ActionType::MarkCollected: {
            Reservation* r = rlFindById(caf.reservations, a.reservationId);
            if (r == nullptr || r->status != ResStatus::Collected) return false;
            r->status = ResStatus::Ready;
            r->lastNoticeAt = caf.clock->now();
            // It may still be in the reminder queue (collected entries leave lazily).
            if (!inQueue(caf.reminders, r)) qEnqueue(caf.reminders, r);
            return true;
        }
    }
    return false;
}

Result undoLast(Cafeteria& caf, const std::string& staff, NotificationList& out, StaffAction& undone) {
    (void)staff;
    if (!stPop(caf.history, undone)) return Result::NothingToUndo;
    if (!restore(caf, undone)) {
        stPush(caf.history, undone);  // cannot fail: we just popped
        return Result::UndoBlocked;
    }
    if (undone.type == ActionType::SetDailyLimit || undone.type == ActionType::SetAvailable)
        promoteWaitlist(caf, undone.itemIndex, out);
    return Result::Ok;
}

// a comes before b: by item, then reservation time, then ID.
static bool sortsBefore(const Reservation* a, const Reservation* b) {
    if (a->itemIndex != b->itemIndex) return a->itemIndex < b->itemIndex;
    if (a->reservedAt != b->reservedAt) return a->reservedAt < b->reservedAt;
    return a->id < b->id;
}

int staffReservationsSorted(const Cafeteria& caf, Reservation** outArr, int cap) {
    int today = dayKeyOf(caf.clock->now());
    int n = 0;
    for (Reservation* r = caf.reservations.head; r != nullptr && n < cap; r = r->next) {
        if (r->day == today) outArr[n++] = r;
    }
    // Insertion sort.
    for (int i = 1; i < n; i++) {
        Reservation* key = outArr[i];
        int j = i - 1;
        while (j >= 0 && sortsBefore(key, outArr[j])) {
            outArr[j + 1] = outArr[j];
            j--;
        }
        outArr[j + 1] = key;
    }
    return n;
}

Reservation* staffSearchById(const Cafeteria& caf, int id) {
    return rlFindById(caf.reservations, id);
}

// ---- menu items (redesign) ------------------------------------------------------

Result addMenuItem(Cafeteria& caf, const std::string& staff, const std::string& category,
                   const std::string& name, ItemKind kind, int priceTaka, int amount, int& newIndex) {
    (void)staff;
    newIndex = -1;
    if (!validName(category) || !validName(name) || menuFindByName(caf.menu, name) != -1)
        return Result::InvalidValue;
    if (priceTaka < 0 || priceTaka > MAX_PRICE_TAKA) return Result::InvalidValue;
    int maxAmount = kind == ItemKind::Fresh ? caf.cookingCapacity : MAX_STOCK;
    if (amount < 0 || amount > maxAmount) return Result::InvalidValue;
    if (caf.menu.count >= MAX_MENU_ITEMS) return Result::MenuFull;

    MenuItem m;
    m.category = category;
    m.name = name;
    m.kind = kind;
    m.stock = kind == ItemKind::Packaged ? amount : 0;
    m.dailyLimit = kind == ItemKind::Fresh ? amount : 0;
    m.reservedToday = 0;
    m.priceTaka = priceTaka;
    m.available = true;
    menuAdd(caf.menu, m);
    newIndex = caf.menu.count - 1;
    return Result::Ok;
}

Result setPrice(Cafeteria& caf, const std::string& staff, int itemIndex, int priceTaka, bool& undoRecorded) {
    undoRecorded = false;
    MenuItem* item = menuAt(caf.menu, itemIndex);
    if (item == nullptr) return Result::BadItem;
    if (priceTaka < 0 || priceTaka > MAX_PRICE_TAKA) return Result::InvalidValue;
    StaffAction a = newAction(caf, ActionType::SetPrice, staff);
    a.itemIndex = itemIndex;
    a.oldValue = item->priceTaka;
    a.newValue = priceTaka;
    a.description = "Set " + item->name + " price " + std::to_string(a.oldValue) + " -> " + std::to_string(priceTaka);
    item->priceTaka = priceTaka;
    undoRecorded = stPush(caf.history, a);
    return Result::Ok;
}

Result setAvailable(Cafeteria& caf, const std::string& staff, int itemIndex, bool available,
                    NotificationList& out, bool& undoRecorded) {
    undoRecorded = false;
    MenuItem* item = menuAt(caf.menu, itemIndex);
    if (item == nullptr) return Result::BadItem;
    StaffAction a = newAction(caf, ActionType::SetAvailable, staff);
    a.itemIndex = itemIndex;
    a.oldValue = item->available ? 1 : 0;
    a.newValue = available ? 1 : 0;
    a.description = "Made " + item->name + (available ? " available" : " unavailable");
    item->available = available;
    undoRecorded = stPush(caf.history, a);
    if (available) promoteWaitlist(caf, itemIndex, out);
    return Result::Ok;
}

Result setItemImage(Cafeteria& caf, const std::string& staff, int itemIndex, const std::string& fileName) {
    (void)staff;
    MenuItem* item = menuAt(caf.menu, itemIndex);
    if (item == nullptr) return Result::BadItem;
    item->imageFile = fileName;
    return Result::Ok;
}
