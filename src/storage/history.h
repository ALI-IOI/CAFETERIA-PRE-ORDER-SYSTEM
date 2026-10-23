// A person's order history: finished reservations from earlier days, which the
// day rollover appends to data/history.txt (one line per reservation, same
// format as reservations.txt).
#pragma once
#include "ds/reservation_list.h"
#include <string>

// Reads history.txt, appends this user's records to `out` (a linked list of
// new nodes the caller frees with rlClear) and sorts it newest first with the
// list merge sort. A missing file means no history.
bool loadUserHistory(const std::string& dataDir, const std::string& user, ReservationList& out, std::string& err);
