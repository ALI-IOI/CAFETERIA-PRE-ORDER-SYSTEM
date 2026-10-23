// Circular queue of reservation pointers (proposal section 7). Used for each
// fresh item's cooking queue and for the reminder queue. Enqueue/dequeue O(1).
//
// The queue never owns the reservations. Cancelled or collected reservations
// stay where they are and are skipped when they reach the front.
#pragma once
#include "ds/reservation_list.h"

struct ResQueue {
    Reservation** slots;  // array of `capacity` pointers, allocated with new[]
    int capacity;
    int front;            // index of the first element
    int rear;             // index where the next element will be written
    int count;
};

// Allocates the slot array. Returns false for capacity <= 0.
bool qInit(ResQueue& q, int capacity);
void qFree(ResQueue& q);

bool qIsEmpty(const ResQueue& q);
bool qIsFull(const ResQueue& q);

// Returns false (and changes nothing) when the queue is full.
bool qEnqueue(ResQueue& q, Reservation* r);

// Returns nullptr when the queue is empty.
Reservation* qDequeue(ResQueue& q);
Reservation* qPeek(const ResQueue& q);

// i-th element counted from the front (for displaying the queue); nullptr if out of range.
Reservation* qAt(const ResQueue& q, int i);

// Removes every element but keeps the slot array.
void qClear(ResQueue& q);
