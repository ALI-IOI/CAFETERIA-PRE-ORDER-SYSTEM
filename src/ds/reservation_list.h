// Reservations: a singly linked list with head and tail pointers
// (proposal section 7). Insert at tail is O(1); search/delete are O(n).
#pragma once
#include <ctime>
#include <string>

enum class ResStatus { Reserved, Ready, Collected, Cancelled, Expired };

struct Reservation {
    int id;
    std::string user;      // login (university email) of the person who reserved
    int itemIndex;         // index into the menu array
    int quantity;
    time_t reservedAt;
    ResStatus status;
    time_t readyAt;        // 0 until staff mark it ready
    time_t lastNoticeAt;   // time of the last ready/reminder email
    int day;               // YYYYMMDD the reservation belongs to
    Reservation* next;
};

struct ReservationList {
    Reservation* head;
    Reservation* tail;
    int size;
    int nextId;
};

void rlInit(ReservationList& list);

// Creates a node with the next ID and status Reserved, links it at the tail. O(1).
Reservation* rlAppend(ReservationList& list, const std::string& user, int itemIndex,
                      int quantity, time_t at, int day);

// Links an already-filled node at the tail (used when loading from file). O(1).
void rlAppendNode(ReservationList& list, Reservation* node);

Reservation* rlFindById(const ReservationList& list, int id);

// Unlinks and frees the node. Returns false when the ID is not in the list.
bool rlDelete(ReservationList& list, int id);

// Frees every node.
void rlClear(ReservationList& list);

const char* statusName(ResStatus s);
bool statusFromName(const std::string& name, ResStatus& out);

// "R-0042" style code shown to users.
std::string resCode(int id);
