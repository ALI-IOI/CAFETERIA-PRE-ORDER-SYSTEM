#include "storage/history.h"
#include "storage/file_util.h"
#include <fstream>

bool loadUserHistory(const std::string& dataDir, const std::string& user, ReservationList& out, std::string& err) {
    std::ifstream in(dataDir + "/history.txt");
    if (!in) return true;
    std::string line, f[11];
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        int n = splitFields(line, '|', f, 11);
        Reservation r;
        long long reservedAt;
        // Only the first nine fields are needed here; both file formats have them.
        bool ok = (n == 9 || n == 11) && parseIntStrict(f[0], r.id) && parseIntStrict(f[2], r.itemIndex) &&
                  parseIntStrict(f[3], r.quantity) && parseIntStrict(f[4], reservedAt) &&
                  statusFromName(f[5], r.status) && parseIntStrict(f[8], r.day);
        if (!ok) {
            err = "history.txt line " + std::to_string(lineNo) + ": bad record";
            return false;
        }
        if (f[1] != user) continue;
        Reservation* node = new Reservation(r);
        node->user = f[1];
        node->reservedAt = (time_t)reservedAt;
        node->readyAt = node->lastNoticeAt = node->expiresAt = 0;
        node->reminderMinutes = 0;
        rlAppendNode(out, node);
    }
    rlSortNewestFirst(out);
    return true;
}
