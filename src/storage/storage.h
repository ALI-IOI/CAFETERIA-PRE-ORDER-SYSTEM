// Text-file persistence for the cafeteria (proposal sections 6 and 8).
//
//   menu.txt          category|name|kind|stock|dailyLimit|priceTaka|imageFile|available
//                     (array order = item index; older 5-field lines still load)
//   settings.txt      key=value  (settings + current_day + next_id)
//   reservations.txt  id|user|item|qty|reservedAt|status|readyAt|lastNoticeAt|day|expiresAt|reminderMinutes
//                     (older 9-field lines still load; the last two are filled from settings)
//   history.txt       same format, append-only archive of earlier days
//   waitlist.txt      day|item|user|qty|joinedAt  (today's active waitlist entries, front first)
//
// Lines starting with '#' and blank lines are ignored.
#pragma once
#include "core/cafeteria.h"
#include <string>

// Loads `dataDir` into a cafInit()-ed Cafeteria. If dataDir has no menu.txt,
// the menu comes from seedDir/menu.txt (and settings from seedDir/settings.txt
// if present). Then rebuilds the queues: cooking queues from Reserved
// reservations in list order, the reminder queue from Ready reservations
// sorted by their next due time. The action stack stays empty.
// On a malformed line returns false with err = "<file> line <n>: <reason>".
bool loadCafeteria(Cafeteria& caf, const std::string& dataDir, const std::string& seedDir,
                   std::string& err);

// Writes menu, settings and reservations atomically, and appends the archive
// list to history.txt (then empties the archive).
bool saveCafeteria(Cafeteria& caf, const std::string& dataDir, std::string& err);
