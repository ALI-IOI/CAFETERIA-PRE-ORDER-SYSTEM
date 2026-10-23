// The cafeteria's own computers: the only ones that can reach the staff terminal.
#include "testing.h"
#include "auth/staff_computers.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string writeList(const char* name, const std::string& text) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests";
    fs::create_directories(dir);
    std::string path = (dir / name).string();
    std::ofstream o(path);
    o << text;
    return path;
}

TEST(staff_computers_missing_file_is_empty) {
    StaffComputers sc;
    staffComputersInit(sc);
    std::string err;
    CHECK(staffComputersLoad(sc, (fs::temp_directory_path() / "cafeteria_tests" / "no_such_list.txt").string(), err));
    CHECK_EQ(sc.count, 0);
}

TEST(staff_computers_comments_and_blanks) {
    StaffComputers sc;
    staffComputersInit(sc);
    std::string err;
    CHECK(staffComputersLoad(sc, writeList("sc_ok.txt", "# counter PCs\n\n192.168.10.21\r\n  \nfe80::1\n"), err));
    CHECK_EQ(sc.count, 2);
    CHECK(sc.addr[0] == "192.168.10.21" && sc.addr[1] == "fe80::1");
}

TEST(staff_computers_bad_line_refused) {
    StaffComputers sc;
    staffComputersInit(sc);
    std::string err;
    CHECK(!staffComputersLoad(sc, writeList("sc_bad.txt", "# list\n10.0.0.5\n192.168.1.300\n"), err));
    CHECK(err.find("line 3") != std::string::npos);
    CHECK(!staffComputersLoad(sc, writeList("sc_bad2.txt", "counter-pc\n"), err));
    CHECK(!staffComputersLoad(sc, writeList("sc_bad3.txt", "010.0.0.5\n"), err));
    CHECK(!staffComputersLoad(sc, writeList("sc_bad4.txt", "10.0.0\n"), err));
}

TEST(staff_computers_too_many_refused) {
    std::string text;
    for (int i = 1; i <= MAX_STAFF_COMPUTERS + 1; i++) text += "10.0.0." + std::to_string(i) + "\n";
    StaffComputers sc;
    staffComputersInit(sc);
    std::string err;
    CHECK(!staffComputersLoad(sc, writeList("sc_many.txt", text), err));
    CHECK(err.find("line 17") != std::string::npos);
}

TEST(staff_computer_empty_list_allows_all) {
    StaffComputers sc;
    staffComputersInit(sc);
    CHECK(staffComputerAllowed(sc, "203.0.113.9"));
}

TEST(staff_computer_exact_match) {
    StaffComputers sc;
    staffComputersInit(sc);
    std::string err;
    CHECK(staffComputersLoad(sc, writeList("sc_one.txt", "10.0.0.5\n"), err));
    CHECK(staffComputerAllowed(sc, "10.0.0.5"));
    CHECK(!staffComputerAllowed(sc, "10.0.0.50"));
    CHECK(!staffComputerAllowed(sc, "127.0.0.1"));
    CHECK(!staffComputerAllowed(sc, ""));
}

TEST(staff_computer_ipv4_mapped_matches) {
    StaffComputers sc;
    staffComputersInit(sc);
    std::string err;
    CHECK(staffComputersLoad(sc, writeList("sc_mapped.txt", "10.0.0.5\n"), err));
    CHECK(staffComputerAllowed(sc, "::ffff:10.0.0.5"));
    CHECK(!staffComputerAllowed(sc, "::ffff:10.0.0.6"));
}

TEST(is_staff_path_cases) {
    CHECK(isStaffPath("/staff/login.html"));
    CHECK(isStaffPath("/STAFF/portal.html"));
    CHECK(isStaffPath("/staff"));
    CHECK(isStaffPath("/api/staff"));
    CHECK(isStaffPath("/api/Staff/overview"));
    CHECK(!isStaffPath("/staffing.html"));
    CHECK(!isStaffPath("/api/me"));
    CHECK(!isStaffPath("/menu.html"));
    CHECK(!isStaffPath("/api/staffroom"));
}
