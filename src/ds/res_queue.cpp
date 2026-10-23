#include "ds/res_queue.h"

bool qInit(ResQueue& q, int capacity) {
    q.slots = nullptr;
    q.capacity = 0;
    q.front = 0;
    q.rear = 0;
    q.count = 0;
    if (capacity <= 0) return false;
    q.slots = new Reservation*[capacity];
    q.capacity = capacity;
    return true;
}

void qFree(ResQueue& q) {
    delete[] q.slots;
    q.slots = nullptr;
    q.capacity = 0;
    q.front = 0;
    q.rear = 0;
    q.count = 0;
}

bool qIsEmpty(const ResQueue& q) {
    return q.count == 0;
}

bool qIsFull(const ResQueue& q) {
    return q.count == q.capacity;
}

bool qEnqueue(ResQueue& q, Reservation* r) {
    if (qIsFull(q)) return false;
    q.slots[q.rear] = r;
    q.rear = (q.rear + 1) % q.capacity;
    q.count++;
    return true;
}

Reservation* qDequeue(ResQueue& q) {
    if (qIsEmpty(q)) return nullptr;
    Reservation* r = q.slots[q.front];
    q.front = (q.front + 1) % q.capacity;
    q.count--;
    return r;
}

Reservation* qPeek(const ResQueue& q) {
    if (qIsEmpty(q)) return nullptr;
    return q.slots[q.front];
}

Reservation* qAt(const ResQueue& q, int i) {
    if (i < 0 || i >= q.count) return nullptr;
    return q.slots[(q.front + i) % q.capacity];
}

void qClear(ResQueue& q) {
    q.front = 0;
    q.rear = 0;
    q.count = 0;
}
