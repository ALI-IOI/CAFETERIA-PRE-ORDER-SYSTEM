// Console program for proving the data structures before the web layer
// (proposal section 14, stage 2). It drives the same core functions the server
// uses, on a fake clock, and shows the queues and the stack directly.
//
// Commands (type "help"): menu, reserve, cancel, mine, ready, collected, stock,
// limit, set, undo, history, queues, list, time, advance, quit.
#include "core/cafeteria.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

static void addItem(Cafeteria& caf, const char* cat, const char* name, ItemKind kind,
                    int stock, int limit) {
    MenuItem m;
    m.category = cat;
    m.name = name;
    m.kind = kind;
    m.stock = stock;
    m.dailyLimit = limit;
    m.reservedToday = 0;
    menuAdd(caf.menu, m);
}

// Demo values only; the server reads the real menu from data files.
static void loadDemoMenu(Cafeteria& caf) {
    addItem(caf, "Fresh food", "Khichuri", ItemKind::Fresh, 0, 12);
    addItem(caf, "Fresh food", "Fried Rice", ItemKind::Fresh, 0, 8);
    addItem(caf, "Fresh food", "Tehari", ItemKind::Fresh, 0, 10);
    addItem(caf, "Chips", "Lays", ItemKind::Packaged, 20, 0);
    addItem(caf, "Chips", "Alloz", ItemKind::Packaged, 15, 0);
    addItem(caf, "Chips", "Mr. Twist", ItemKind::Packaged, 15, 0);
    addItem(caf, "Chips", "Sun", ItemKind::Packaged, 10, 0);
    addItem(caf, "Soft drinks", "Dew", ItemKind::Packaged, 12, 0);
    addItem(caf, "Soft drinks", "Pepsi", ItemKind::Packaged, 12, 0);
    addItem(caf, "Soft drinks", "Mirinda", ItemKind::Packaged, 12, 0);
    addItem(caf, "Soft drinks", "7UP", ItemKind::Packaged, 12, 0);
    addItem(caf, "Soft drinks", "Coca-Cola", ItemKind::Packaged, 12, 0);
}

static bool toInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0' || v < -1000000 || v > 1000000) return false;
    out = (int)v;
    return true;
}

static void printNotifications(NotificationList& out) {
    for (Notification* n = out.head; n != nullptr; n = n->next) {
        std::printf("  [email:%s] To %s: %s\n", notifTypeName(n->type), n->to.c_str(),
                    n->subject.c_str());
        std::istringstream body(n->body);
        std::string line;
        while (std::getline(body, line)) std::printf("      %s\n", line.c_str());
    }
    nlClear(out);
}

static void printMenu(const Cafeteria& caf) {
    std::printf("Fresh food (reserve before %s)\n", formatMinuteOfDay(caf.settings.cutoffMinute).c_str());
    for (int i = 0; i < caf.menu.count; i++) {
        const MenuItem& m = caf.menu.items[i];
        if (m.kind == ItemKind::Fresh)
            std::printf("  [%d] %s: %d portions left (limit %d)\n", i, m.name.c_str(), menuRemaining(m),
                        m.dailyLimit);
    }
    std::printf("Packaged food (view only)\n");
    for (int i = 0; i < caf.menu.count; i++) {
        const MenuItem& m = caf.menu.items[i];
        if (m.kind == ItemKind::Packaged)
            std::printf("  [%d] %s (%s): %d in stock\n", i, m.name.c_str(), m.category.c_str(), m.stock);
    }
}

static void printQueue(const char* title, const ResQueue& q) {
    std::printf("%s (front -> rear): ", title);
    if (qIsEmpty(q)) std::printf("(empty)");
    for (int i = 0; i < q.count; i++) {
        Reservation* r = qAt(q, i);
        std::printf("%s%s %s %s", i ? " | " : "", resCode(r->id).c_str(), r->user.c_str(),
                    statusName(r->status));
    }
    std::printf("   [%d/%d]\n", q.count, q.capacity);
}

static void printReservation(const Cafeteria& caf, const Reservation* r) {
    const MenuItem* m = menuAt(caf.menu, r->itemIndex);
    std::printf("  %s  %-10s  %-10s x%d  reserved %s  %s\n", resCode(r->id).c_str(), r->user.c_str(),
                m ? m->name.c_str() : "?", r->quantity, formatClock(r->reservedAt).c_str(),
                statusName(r->status));
}

static void printHistory(const Cafeteria& caf) {
    if (stIsEmpty(caf.history)) {
        std::printf("Staff action stack is empty\n");
        return;
    }
    std::printf("Staff action stack (top first, %d/%d):\n", stSize(caf.history), ACTION_STACK_CAPACITY);
    for (int i = 0; i < stSize(caf.history); i++) {
        std::printf("  %d. %s\n", i + 1, stFromTop(caf.history, i)->description.c_str());
    }
}

static void reportUndo(bool recorded) {
    if (!recorded) std::printf("  (undo history full - this action cannot be undone)\n");
}

static void printHelp() {
    std::printf(
        "Commands:\n"
        "  menu                         show the menu array\n"
        "  reserve <user> <item> <qty>  reserve a fresh item\n"
        "  cancel <user> <id>           cancel your reservation\n"
        "  mine <user>                  your reservations (linked list traversal)\n"
        "  ready <item> [portions]      staff: mark cooked portions ready (FIFO)\n"
        "  collected <id>               staff: mark a reservation collected\n"
        "  stock <item> <n>             staff: set packaged stock\n"
        "  limit <item> <n>             staff: set fresh daily limit\n"
        "  set <key> <value>            staff: cutoff, cancel_deadline (minutes after midnight),\n"
        "                               freshness_minutes, reminder_minutes, per_user_limit\n"
        "  undo                         staff: undo the latest action (stack pop)\n"
        "  history                      show the staff action stack\n"
        "  queues                       show cooking and reminder queues\n"
        "  list                         staff: today's reservations sorted by item and time\n"
        "  time <HH:MM>                 set the demo clock (same day)\n"
        "  advance <minutes>            move the clock forward, running the timer each minute\n"
        "  quit\n");
}

int main() {
    FakeClock clock;
    clock.set(2026, 10, 23, 8, 0, 0);
    Cafeteria caf;
    cafInit(caf, &clock, 100);
    loadDemoMenu(caf);
    NotificationList out;
    nlInit(out);
    const std::string staff = "staff-console";

    std::printf("Cafeteria pre-order console demo. Clock: %s. Type help.\n", formatClock(clock.now()).c_str());
    std::string line;
    while (true) {
        std::printf("> ");
        if (!std::getline(std::cin, line)) break;
        std::istringstream in(line);
        std::string cmd, a, b, c;
        in >> cmd >> a >> b >> c;
        if (cmd.empty()) continue;
        std::printf("%s\n", line.c_str());  // echo, so scripted runs read like a session

        int x = 0, y = 0;
        bool recorded = false;
        if (cmd == "quit") {
            break;
        } else if (cmd == "help") {
            printHelp();
        } else if (cmd == "menu") {
            printMenu(caf);
        } else if (cmd == "reserve") {
            if (a.empty() || !toInt(b, x) || !toInt(c, y)) {
                std::printf("Usage: reserve <user> <item> <qty> (numbers only)\n");
                continue;
            }
            Result r = reserve(caf, a, x, y, out, nullptr);
            if (r != Result::Ok) std::printf("Refused: %s\n", resultMessage(r));
            printNotifications(out);
        } else if (cmd == "cancel") {
            if (a.empty() || !toInt(b, x)) {
                std::printf("Usage: cancel <user> <id>\n");
                continue;
            }
            Result r = cancelReservation(caf, a, x);
            if (r == Result::Ok) std::printf("Cancelled %s\n", resCode(x).c_str());
            else std::printf("Refused: %s\n", resultMessage(r));
        } else if (cmd == "mine") {
            Reservation* arr[256];
            int n = listUserReservations(caf, a, arr, 256);
            if (n == 0) std::printf("No reservations for %s\n", a.c_str());
            for (int i = 0; i < n; i++) printReservation(caf, arr[i]);
        } else if (cmd == "ready") {
            if (!toInt(a, x) || (!b.empty() && !toInt(b, y))) {
                std::printf("Usage: ready <item> [portions]\n");
                continue;
            }
            ReadyReport rep;
            Result r = markReady(caf, staff, x, b.empty() ? 0 : y, out, rep);
            if (r == Result::Ok)
                std::printf("Ready: %d made ready, %d cancelled skipped, %d still waiting\n", rep.madeReady,
                            rep.skipped, rep.leftWaiting);
            else std::printf("Refused: %s\n", resultMessage(r));
            printNotifications(out);
        } else if (cmd == "collected") {
            if (!toInt(a, x)) {
                std::printf("Usage: collected <id>\n");
                continue;
            }
            Result r = markCollected(caf, staff, x, recorded);
            if (r == Result::Ok) {
                std::printf("Collected %s\n", resCode(x).c_str());
                reportUndo(recorded);
            } else {
                std::printf("Refused: %s\n", resultMessage(r));
            }
        } else if (cmd == "stock" || cmd == "limit") {
            if (!toInt(a, x) || !toInt(b, y)) {
                std::printf("Usage: %s <item> <n>\n", cmd.c_str());
                continue;
            }
            Result r = cmd == "stock" ? setStock(caf, staff, x, y, recorded)
                                      : setDailyLimit(caf, staff, x, y, recorded);
            if (r == Result::Ok) {
                std::printf("Done: %s\n", stIsEmpty(caf.history) ? "" : stPeek(caf.history)->description.c_str());
                reportUndo(recorded);
            } else {
                std::printf("Refused: %s\n", resultMessage(r));
            }
        } else if (cmd == "set") {
            if (a.empty() || !toInt(b, y)) {
                std::printf("Usage: set <key> <value>\n");
                continue;
            }
            Result r = changeSetting(caf, staff, a, y, recorded);
            if (r == Result::Ok) {
                std::printf("Done: %s\n", stPeek(caf.history) ? stPeek(caf.history)->description.c_str() : "");
                reportUndo(recorded);
            } else {
                std::printf("Refused: %s\n", resultMessage(r));
            }
        } else if (cmd == "undo") {
            StaffAction undone;
            Result r = undoLast(caf, staff, undone);
            if (r == Result::Ok) std::printf("Undid: %s\n", undone.description.c_str());
            else if (r == Result::NothingToUndo) std::printf("Nothing to undo (stack is empty)\n");
            else std::printf("Refused: %s\n", resultMessage(r));
        } else if (cmd == "history") {
            printHistory(caf);
        } else if (cmd == "queues") {
            for (int i = 0; i < caf.menu.count; i++) {
                if (caf.menu.items[i].kind != ItemKind::Fresh) continue;
                std::string title = "Cooking queue " + caf.menu.items[i].name;
                printQueue(title.c_str(), caf.cooking[i]);
            }
            printQueue("Reminder queue", caf.reminders);
        } else if (cmd == "list") {
            Reservation* arr[256];
            int n = staffReservationsSorted(caf, arr, 256);
            if (n == 0) std::printf("No reservations today\n");
            for (int i = 0; i < n; i++) printReservation(caf, arr[i]);
        } else if (cmd == "time") {
            int hh, mm;
            if (std::sscanf(a.c_str(), "%d:%d", &hh, &mm) != 2 || hh < 0 || hh > 23 || mm < 0 || mm > 59) {
                std::printf("Usage: time HH:MM\n");
                continue;
            }
            clock.t += (time_t)(hh * 3600 + mm * 60 - secondOfDay(clock.now()));
            std::printf("Clock: %s\n", formatClock(clock.now()).c_str());
        } else if (cmd == "advance") {
            if (!toInt(a, x) || x < 1 || x > 7 * 24 * 60) {
                std::printf("Usage: advance <minutes> (1..10080)\n");
                continue;
            }
            for (int i = 0; i < x; i++) {
                clock.advanceMinutes(1);
                timerTick(caf, out);
                if (out.count > 0) {
                    std::printf("-- %s --\n", formatClock(clock.now()).c_str());
                    printNotifications(out);
                }
            }
            std::printf("Clock: %s\n", formatClock(clock.now()).c_str());
        } else {
            std::printf("Unknown command: %s (type help)\n", cmd.c_str());
        }
    }
    std::printf("Bye\n");
    nlClear(out);
    cafFree(caf);
    return 0;
}
