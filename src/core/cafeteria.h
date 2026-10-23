// The cafeteria rules (proposal section 8). Everything here works only on the
// hand-built structures in ds/. Callers must hold the server mutex.
#pragma once
#include "core/clock.h"
#include "core/notification.h"
#include "core/settings.h"
#include "ds/action_stack.h"
#include "ds/menu_array.h"
#include "ds/res_queue.h"
#include "ds/reservation_list.h"

struct Cafeteria {
    MenuArray menu;                   // array: menu and stock
    ReservationList reservations;     // linked list: every reservation
    ResQueue cooking[MAX_MENU_ITEMS]; // one circular queue per menu slot (used for fresh items)
    ResQueue reminders;               // ready reservations waiting for their next reminder
    ActionStack history;              // staff actions that can be undone
    ReservationList archive;          // finished reservations from earlier days, waiting to be written to history.txt
    Settings settings;
    Clock* clock;
    int currentDay;                   // YYYYMMDD the counters belong to
    int cookingCapacity;
};

enum class Result {
    Ok,
    BadItem,
    NotFresh,
    BadQuantity,
    CutoffPassed,
    SoldOut,
    UserLimit,
    QueueFull,
    NotFound,
    NotOwner,
    NotCancellable,
    DeadlinePassed,
    InvalidValue,
    NothingToUndo,
    UndoBlocked,
    NotReady,
    WrongKind,
};

const char* resultMessage(Result r);

void cafInit(Cafeteria& caf, Clock* clock, int cookingCapacity);
void cafFree(Cafeteria& caf);

// Checks, in order: BadItem, NotFresh, BadQuantity, CutoffPassed, SoldOut,
// UserLimit, QueueFull. On Ok: new node at the list tail, portions taken from
// the array, reservation enqueued in the item's cooking queue, confirmation queued.
Result reserve(Cafeteria& caf, const std::string& user, int itemIndex, int quantity,
               NotificationList& out, Reservation** created);

// Allowed while the reservation is Reserved and before the cancellation deadline.
// The node stays in the cooking queue marked Cancelled; portions go back to the array.
Result cancelReservation(Cafeteria& caf, const std::string& user, int reservationId);

// Portions this user holds today for one item (every status except Cancelled).
int userPortionsToday(const Cafeteria& caf, const std::string& user, int itemIndex);

// Fills outArr with the user's own reservations (list order). Returns how many.
int listUserReservations(const Cafeteria& caf, const std::string& user,
                         Reservation** outArr, int cap);

// True while now is before the given minute-of-day today.
bool beforeMinuteOfDay(const Cafeteria& caf, int minuteOfDay);

struct ReadyReport {
    int madeReady;    // reservations set to Ready
    int skipped;      // cancelled entries dequeued and dropped
    int leftWaiting;  // entries still in the cooking queue afterwards
};

// Staff cooked `portions` portions of a fresh item (0 = everything waiting).
// Dequeues the cooking queue in FIFO order, skips non-Reserved entries, and
// stops when the next reservation needs more portions than are left (it stays
// at the front). Each one becomes Ready, gets a "ready" email and joins the
// rear of the reminder queue. Not undoable (the emails are already sent).
Result markReady(Cafeteria& caf, const std::string& staff, int itemIndex, int portions,
                 NotificationList& out, ReadyReport& report);

// Ready -> Collected. Pushes an undoable action; undoRecorded is false when the
// undo stack was full (the action still happens).
Result markCollected(Cafeteria& caf, const std::string& staff, int reservationId,
                     bool& undoRecorded);

// Called by the timer every minute: day rollover, then reminders and expiry
// from the front of the reminder queue.
void timerTick(Cafeteria& caf, NotificationList& out);

// Runs the day rollover (D9) if the date changed since the counters were last
// reset. reserve/cancel/markReady/setDailyLimit call it first, so nothing made
// after midnight is counted against yesterday or wiped by a later tick.
void cafEnsureToday(Cafeteria& caf);

// "7 hours", "1 hour 30 minutes", "45 minutes".
std::string durationText(int minutes);

// ---- staff portal (staff.cpp) ----------------------------------------------
// Each setter applies the change and pushes an undoable action. undoRecorded is
// false when the stack was full (D5: the change still happens).

const int MAX_STOCK = 9999;

// Packaged items only; 0..MAX_STOCK.
Result setStock(Cafeteria& caf, const std::string& staff, int itemIndex, int value,
                bool& undoRecorded);

// Fresh items only; portions already reserved today <= value <= cooking queue capacity.
Result setDailyLimit(Cafeteria& caf, const std::string& staff, int itemIndex, int value,
                     bool& undoRecorded);

// Keys and ranges as in settingsSet().
Result changeSetting(Cafeteria& caf, const std::string& staff, const std::string& key,
                     int value, bool& undoRecorded);

// Pops the latest action and restores its old value. If restoring would break a
// rule (limit below portions now reserved, reservation no longer Collected) the
// action is pushed back and UndoBlocked is returned.
Result undoLast(Cafeteria& caf, const std::string& staff, StaffAction& undone);

// Today's reservations sorted by item, then reservation time (insertion sort, O(k^2)).
int staffReservationsSorted(const Cafeteria& caf, Reservation** outArr, int cap);

Reservation* staffSearchById(const Cafeteria& caf, int id);
