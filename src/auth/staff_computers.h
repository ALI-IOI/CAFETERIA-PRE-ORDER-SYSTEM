// The cafeteria's own computers (config/staff_computers.txt): while the list has
// entries, only these addresses can reach the staff terminal; everyone else gets
// the same answer as a page that does not exist. Kept in a fixed array.
#pragma once
#include <string>

const int MAX_STAFF_COMPUTERS = 16;  // [CONFIRM] enough for the counter PCs

struct StaffComputers {
    std::string addr[MAX_STAFF_COMPUTERS];  // lower-case IPv4 or IPv6 addresses
    int count;
};

void staffComputersInit(StaffComputers& sc);

// One address per line; '#' lines and blank lines skipped. Missing file = empty
// list. A malformed address or more than MAX_STAFF_COMPUTERS entries -> false,
// err = "<path> line <n>: <reason>".
bool staffComputersLoad(StaffComputers& sc, const std::string& path, std::string& err);

// Empty list -> true. Otherwise exact match; "::ffff:a.b.c.d" (an IPv4 address
// seen through an IPv6 socket) also matches "a.b.c.d".
bool staffComputerAllowed(const StaffComputers& sc, const std::string& remoteAddr);

// True when any segment of the path is "staff", ignoring case ('/' and '\' both
// separate segments): "/staff/login.html", "/STAFF/portal.html", "/api/staff",
// "/x/../staff/login.html". Windows serves files case-insensitively, so case
// must not matter.
bool isStaffPath(const std::string& path);
