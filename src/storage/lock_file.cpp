#include "storage/lock_file.h"
#include "storage/file_util.h"
#include <cstdio>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

static long long currentPid() {
#ifdef _WIN32
    return (long long)GetCurrentProcessId();
#else
    return (long long)getpid();
#endif
}

static bool processAlive(long long pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (h == nullptr) return GetLastError() == ERROR_ACCESS_DENIED;  // exists but protected
    DWORD code = 0;
    bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
#else
    return kill((pid_t)pid, 0) == 0;
#endif
}

LockState lockStatus(const std::string& path) {
    std::ifstream in(path);
    if (!in) return LockState::None;
    std::string line;
    std::getline(in, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    long long pid;
    if (!parseIntStrict(line, pid) || pid <= 0 || pid > 0xFFFFFFFFLL) return LockState::Live;
    if (pid == currentPid()) return LockState::Mine;
    return processAlive(pid) ? LockState::Live : LockState::Stale;
}

bool acquireLock(const std::string& path, std::string& err) {
    if (lockStatus(path) == LockState::Live) {
        std::ifstream in(path);
        std::string holder;
        std::getline(in, holder);
        err = "another server (PID " + holder + ") is using this data folder; stop it first, or delete " + path +
              " if you are sure it is not running";
        return false;
    }
    return atomicWrite(path, std::to_string(currentPid()) + "\n", err);
}

void releaseLock(const std::string& path) {
    if (lockStatus(path) == LockState::Mine) std::remove(path.c_str());
}
