#include "storage/storage.h"
#include "storage/file_util.h"
#include <fstream>

static std::string lineError(const char* file, int lineNo, const std::string& why) {
    return std::string(file) + " line " + std::to_string(lineNo) + ": " + why;
}

static bool isSkippable(const std::string& line) {
    return line.empty() || line[0] == '#' || line == "\r";
}

static std::string stripCR(const std::string& s) {
    return (!s.empty() && s.back() == '\r') ? s.substr(0, s.size() - 1) : s;
}

static bool loadMenu(Cafeteria& caf, const std::string& path, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "cannot open " + path;
        return false;
    }
    std::string line, f[5];
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        line = stripCR(line);
        if (isSkippable(line)) continue;
        MenuItem m;
        if (splitFields(line, '|', f, 5) != 5 || f[1].empty()) {
            err = lineError("menu.txt", lineNo, "expected category|name|kind|stock|dailyLimit");
            return false;
        }
        m.category = f[0];
        m.name = f[1];
        if (f[2] == "fresh") m.kind = ItemKind::Fresh;
        else if (f[2] == "packaged") m.kind = ItemKind::Packaged;
        else {
            err = lineError("menu.txt", lineNo, "kind must be fresh or packaged");
            return false;
        }
        if (!parseIntStrict(f[3], m.stock) || !parseIntStrict(f[4], m.dailyLimit) || m.stock < 0 ||
            m.dailyLimit < 0) {
            err = lineError("menu.txt", lineNo, "stock and dailyLimit must be whole numbers >= 0");
            return false;
        }
        m.reservedToday = 0;
        if (!menuAdd(caf.menu, m)) {
            err = lineError("menu.txt", lineNo, "too many items (max " + std::to_string(MAX_MENU_ITEMS) + ")");
            return false;
        }
    }
    return true;
}

static bool loadSettings(Cafeteria& caf, const std::string& path, std::string& err) {
    std::ifstream in(path);
    if (!in) return true;  // optional: defaults stay
    std::string line, f[2];
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        line = stripCR(line);
        if (isSkippable(line)) continue;
        int value;
        if (splitFields(line, '=', f, 2) != 2 || !parseIntStrict(f[1], value)) {
            err = lineError("settings.txt", lineNo, "expected key=number");
            return false;
        }
        if (f[0] == "current_day") caf.currentDay = value;
        else if (f[0] == "next_id") caf.reservations.nextId = value;
        else if (!settingsSet(caf.settings, f[0], value)) {
            err = lineError("settings.txt", lineNo, "unknown key or value out of range");
            return false;
        }
    }
    return true;
}

static bool loadReservations(Cafeteria& caf, const std::string& path, std::string& err) {
    std::ifstream in(path);
    if (!in) return true;  // no reservations yet
    std::string line, f[9];
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        line = stripCR(line);
        if (isSkippable(line)) continue;
        if (splitFields(line, '|', f, 9) != 9) {
            err = lineError("reservations.txt", lineNo, "expected 9 fields");
            return false;
        }
        Reservation* r = new Reservation;
        long long reservedAt, readyAt, lastNoticeAt;
        bool ok = parseIntStrict(f[0], r->id) && parseIntStrict(f[2], r->itemIndex) &&
                  parseIntStrict(f[3], r->quantity) && parseIntStrict(f[4], reservedAt) &&
                  statusFromName(f[5], r->status) && parseIntStrict(f[6], readyAt) &&
                  parseIntStrict(f[7], lastNoticeAt) && parseIntStrict(f[8], r->day);
        if (!ok || r->id <= 0 || r->quantity <= 0 || f[1].empty() ||
            menuAt(caf.menu, r->itemIndex) == nullptr || rlFindById(caf.reservations, r->id)) {
            delete r;
            err = lineError("reservations.txt", lineNo, "bad value, unknown item or duplicate ID");
            return false;
        }
        r->user = f[1];
        r->reservedAt = (time_t)reservedAt;
        r->readyAt = (time_t)readyAt;
        r->lastNoticeAt = (time_t)lastNoticeAt;
        rlAppendNode(caf.reservations, r);  // keeps nextId above every loaded ID
    }
    return true;
}

// Rebuilds everything derived from the reservation list (proposal section 8).
static void rebuild(Cafeteria& caf) {
    for (int i = 0; i < caf.menu.count; i++) {
        caf.menu.items[i].reservedToday = 0;
        qClear(caf.cooking[i]);
    }
    qClear(caf.reminders);
    stInit(caf.history);  // the undo stack starts empty after a restart

    int readyCount = 0;
    for (Reservation* r = caf.reservations.head; r != nullptr; r = r->next) {
        if (r->day == caf.currentDay && r->status != ResStatus::Cancelled)
            caf.menu.items[r->itemIndex].reservedToday += r->quantity;
        if (r->day == caf.currentDay && r->status == ResStatus::Reserved)
            qEnqueue(caf.cooking[r->itemIndex], r);  // list order = arrival order
        if (r->status == ResStatus::Ready) readyCount++;
    }
    if (readyCount == 0) return;

    // Ready reservations, sorted by last notice time (insertion sort), so the
    // earliest-due reminder is at the front again.
    Reservation** ready = new Reservation*[readyCount];
    int n = 0;
    for (Reservation* r = caf.reservations.head; r != nullptr; r = r->next) {
        if (r->status == ResStatus::Ready) ready[n++] = r;
    }
    for (int i = 1; i < n; i++) {
        Reservation* key = ready[i];
        int j = i - 1;
        while (j >= 0 && ready[j]->lastNoticeAt > key->lastNoticeAt) {
            ready[j + 1] = ready[j];
            j--;
        }
        ready[j + 1] = key;
    }
    for (int i = 0; i < n; i++) qEnqueue(caf.reminders, ready[i]);
    delete[] ready;
}

bool loadCafeteria(Cafeteria& caf, const std::string& dataDir, const std::string& seedDir,
                   std::string& err) {
    std::string menuPath = dataDir + "/menu.txt";
    std::string settingsPath = dataDir + "/settings.txt";
    if (!fileExists(menuPath)) {
        if (seedDir.empty()) {
            err = "no menu.txt in " + dataDir + " and no seed directory";
            return false;
        }
        menuPath = seedDir + "/menu.txt";
        settingsPath = seedDir + "/settings.txt";
    }
    int dayBefore = caf.currentDay;
    if (!loadMenu(caf, menuPath, err)) return false;
    if (!loadSettings(caf, settingsPath, err)) return false;
    if (caf.currentDay < 19000101) caf.currentDay = dayBefore;
    if (!loadReservations(caf, dataDir + "/reservations.txt", err)) return false;
    rebuild(caf);
    return true;
}

static std::string reservationLine(const Reservation* r) {
    return std::to_string(r->id) + "|" + r->user + "|" + std::to_string(r->itemIndex) + "|" +
           std::to_string(r->quantity) + "|" + std::to_string((long long)r->reservedAt) + "|" +
           statusName(r->status) + "|" + std::to_string((long long)r->readyAt) + "|" +
           std::to_string((long long)r->lastNoticeAt) + "|" + std::to_string(r->day) + "\n";
}

bool saveCafeteria(Cafeteria& caf, const std::string& dataDir, std::string& err) {
    if (!ensureDir(dataDir, err)) return false;

    std::string menu = "# category|name|kind|stock|dailyLimit  (line order = item index)\n";
    for (int i = 0; i < caf.menu.count; i++) {
        const MenuItem& m = caf.menu.items[i];
        menu += m.category + "|" + m.name + "|" + (m.kind == ItemKind::Fresh ? "fresh" : "packaged") + "|" +
                std::to_string(m.stock) + "|" + std::to_string(m.dailyLimit) + "\n";
    }

    std::string settings;
    for (int i = 0; SETTING_KEYS[i] != nullptr; i++) {
        int v;
        settingsGet(caf.settings, SETTING_KEYS[i], v);
        settings += std::string(SETTING_KEYS[i]) + "=" + std::to_string(v) + "\n";
    }
    settings += "current_day=" + std::to_string(caf.currentDay) + "\n";
    settings += "next_id=" + std::to_string(caf.reservations.nextId) + "\n";

    std::string res = "# id|user|item|qty|reservedAt|status|readyAt|lastNoticeAt|day\n";
    for (Reservation* r = caf.reservations.head; r != nullptr; r = r->next) res += reservationLine(r);

    if (caf.archive.size > 0) {
        std::string hist;
        for (Reservation* r = caf.archive.head; r != nullptr; r = r->next) hist += reservationLine(r);
        if (!appendFile(dataDir + "/history.txt", hist, err)) return false;
        rlClear(caf.archive);
    }
    return atomicWrite(dataDir + "/menu.txt", menu, err) &&
           atomicWrite(dataDir + "/settings.txt", settings, err) &&
           atomicWrite(dataDir + "/reservations.txt", res, err);
}
