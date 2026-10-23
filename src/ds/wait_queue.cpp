#include "ds/wait_queue.h"

bool wqInit(WaitQueue& q, int capacity) {
    q.slots = nullptr;
    q.capacity = 0;
    q.front = 0;
    q.rear = 0;
    q.count = 0;
    if (capacity <= 0) return false;
    q.slots = new WaitEntry[capacity];
    q.capacity = capacity;
    return true;
}

void wqFree(WaitQueue& q) {
    delete[] q.slots;
    q.slots = nullptr;
    q.capacity = 0;
    q.front = 0;
    q.rear = 0;
    q.count = 0;
}

bool wqIsEmpty(const WaitQueue& q) {
    return q.count == 0;
}

bool wqIsFull(const WaitQueue& q) {
    return q.count == q.capacity;
}

bool wqEnqueue(WaitQueue& q, const WaitEntry& e) {
    if (wqIsFull(q)) return false;
    q.slots[q.rear] = e;
    q.rear = (q.rear + 1) % q.capacity;
    q.count++;
    return true;
}

bool wqDequeue(WaitQueue& q, WaitEntry& out) {
    if (wqIsEmpty(q)) return false;
    out = q.slots[q.front];
    q.front = (q.front + 1) % q.capacity;
    q.count--;
    return true;
}

WaitEntry* wqPeek(WaitQueue& q) {
    return wqIsEmpty(q) ? nullptr : &q.slots[q.front];
}

WaitEntry* wqAt(WaitQueue& q, int i) {
    if (i < 0 || i >= q.count) return nullptr;
    return &q.slots[(q.front + i) % q.capacity];
}

const WaitEntry* wqAt(const WaitQueue& q, int i) {
    if (i < 0 || i >= q.count) return nullptr;
    return &q.slots[(q.front + i) % q.capacity];
}

void wqClear(WaitQueue& q) {
    q.front = 0;
    q.rear = 0;
    q.count = 0;
}
