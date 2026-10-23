// Waitlist for a sold-out fresh item: a circular queue of WaitEntry values
// (suggested addition #1). FIFO: the person who joined first gets the next
// free portion. People who leave stay in place marked inactive and are
// skipped when they reach the front, like cancelled cooking-queue entries.
#pragma once
#include <ctime>
#include <string>

struct WaitEntry {
    std::string user;
    int quantity;
    time_t joinedAt;
    bool active;  // false once the person leaves (lazy removal)
};

struct WaitQueue {
    WaitEntry* slots;  // array of `capacity` entries, allocated with new[]
    int capacity;
    int front;
    int rear;
    int count;
};

bool wqInit(WaitQueue& q, int capacity);  // false for capacity <= 0
void wqFree(WaitQueue& q);
bool wqIsEmpty(const WaitQueue& q);
bool wqIsFull(const WaitQueue& q);
bool wqEnqueue(WaitQueue& q, const WaitEntry& e);  // false when full
bool wqDequeue(WaitQueue& q, WaitEntry& out);      // false when empty
WaitEntry* wqPeek(WaitQueue& q);                   // nullptr when empty
WaitEntry* wqAt(WaitQueue& q, int i);              // i-th from the front, nullptr if out of range
const WaitEntry* wqAt(const WaitQueue& q, int i);
void wqClear(WaitQueue& q);
