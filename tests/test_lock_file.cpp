#include "testing.h"
#include "storage/file_util.h"
#include "storage/lock_file.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string lockPath(const char* name) {
    fs::path dir = fs::temp_directory_path() / "cafeteria_tests" / "locks";
    fs::create_directories(dir);
    fs::remove(dir / name);
    return (dir / name).string();
}

static void writeText(const std::string& path, const std::string& text) {
    std::ofstream(path) << text;
}

TEST(lock_absent_then_acquired_with_own_pid) {
    std::string p = lockPath("a.lock");
    CHECK_EQ(lockStatus(p), LockState::None);
    std::string err;
    CHECK(acquireLock(p, err));
    CHECK_EQ(lockStatus(p), LockState::Mine);
    releaseLock(p);
    CHECK(!fileExists(p));
}

TEST(lock_held_by_live_other_process_refused) {
    std::string p = lockPath("b.lock");
    writeText(p, "4\n");  // PID 4 = Windows System process: always alive
    CHECK_EQ(lockStatus(p), LockState::Live);
    std::string err;
    CHECK(!acquireLock(p, err));
    CHECK(err.find("PID 4") != std::string::npos);
    releaseLock(p);  // not ours: must stay
    CHECK(fileExists(p));
}

TEST(stale_lock_from_crashed_process_is_taken_over) {
    std::string p = lockPath("c.lock");
    writeText(p, "4000000\n");  // no such process
    CHECK_EQ(lockStatus(p), LockState::Stale);
    std::string err;
    CHECK(acquireLock(p, err));
    CHECK_EQ(lockStatus(p), LockState::Mine);
    releaseLock(p);
}

TEST(unreadable_lock_treated_as_live) {
    std::string p = lockPath("d.lock");
    writeText(p, "running\n");
    CHECK_EQ(lockStatus(p), LockState::Live);
}
