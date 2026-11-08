#include "storage/history.h"
#include "storage/file_util.h"
#include <fstream>

bool loadUserHistory(const std::string& dataDir, const std::string& user, ReservationList& out, std::string& err) {
    std::ifstream in(dataDir + "/history.txt");
    if (!in) return true;
    std::string line, f[15];
    int lineNo = 0, damaged = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        int n = splitFields(line, '|', f, 15);
        Reservation r;
        long long reservedAt;
        // The first nine fields are in every file format; the token from 12 fields on,
        // the price paid, coupon and discount only in the newest (15 fields).
        r.token = 0;
        r.priceTaka = r.discountTaka = 0;
        bool ok = (n == 9 || n == 11 || n == 12 || n == 15) && (n < 12 || parseIntStrict(f[11], r.token)) &&
                  (n < 15 || (parseIntStrict(f[12], r.priceTaka) && parseIntStrict(f[14], r.discountTaka))) &&
                  parseIntStrict(f[0], r.id) && parseIntStrict(f[2], r.itemIndex) &&
                  parseIntStrict(f[3], r.quantity) && parseIntStrict(f[4], reservedAt) &&
                  statusFromName(f[5], r.status) && parseIntStrict(f[8], r.day);
        if (!ok) {
            // A torn line (a crash during the midnight append) must not hide
            // everyone's history: skip it and report the first one.
            if (damaged++ == 0) err = "history.txt line " + std::to_string(lineNo) + ": bad record";
            continue;
        }
        if (f[1] != user) continue;
        Reservation* node = new Reservation(r);
        node->user = f[1];
        if (n == 15) node->coupon = f[13];
        node->reservedAt = (time_t)reservedAt;
        node->readyAt = node->lastNoticeAt = node->expiresAt = 0;
        node->reminderMinutes = 0;
        rlAppendNode(out, node);
    }
    if (damaged > 0) err += " (" + std::to_string(damaged) + " damaged line(s) skipped)";
    rlSortNewestFirst(out);
    return true;
}
