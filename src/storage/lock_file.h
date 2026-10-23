// data/server.lock holds the PID of the server using that data folder.
// It stops a second server (or cafeteria_admin) from writing the same files,
// while a lock left behind by a crash does not block anyone.
#pragma once
#include <string>

enum class LockState {
    None,   // no lock file
    Mine,   // held by this process
    Live,   // held by another running process (or unreadable: assume live)
    Stale,  // the process that wrote it has exited
};

LockState lockStatus(const std::string& path);

// Writes this process's PID unless another live process holds the lock.
bool acquireLock(const std::string& path, std::string& err);

// Removes the lock only if this process holds it.
void releaseLock(const std::string& path);
