#include "ds/reservation_list.h"
#include <cstdio>

void rlInit(ReservationList& list) {
    list.head = nullptr;
    list.tail = nullptr;
    list.size = 0;
    list.nextId = 1;
}

void rlAppendNode(ReservationList& list, Reservation* node) {
    node->next = nullptr;
    if (list.tail == nullptr) {
        list.head = node;
    } else {
        list.tail->next = node;
    }
    list.tail = node;
    list.size++;
    if (node->id >= list.nextId) list.nextId = node->id + 1;
}

Reservation* rlAppend(ReservationList& list, const std::string& user, int itemIndex,
                      int quantity, time_t at, int day) {
    Reservation* node = new Reservation;
    node->id = list.nextId;
    node->user = user;
    node->itemIndex = itemIndex;
    node->quantity = quantity;
    node->reservedAt = at;
    node->status = ResStatus::Reserved;
    node->readyAt = 0;
    node->lastNoticeAt = 0;
    node->expiresAt = 0;
    node->reminderMinutes = 0;
    node->day = day;
    rlAppendNode(list, node);
    return node;
}

Reservation* rlFindById(const ReservationList& list, int id) {
    for (Reservation* cur = list.head; cur != nullptr; cur = cur->next) {
        if (cur->id == id) return cur;
    }
    return nullptr;
}

Reservation* rlUnlink(ReservationList& list, int id) {
    Reservation* prev = nullptr;
    Reservation* cur = list.head;
    while (cur != nullptr && cur->id != id) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == nullptr) return nullptr;

    if (prev == nullptr) {
        list.head = cur->next;
    } else {
        prev->next = cur->next;
    }
    if (list.tail == cur) list.tail = prev;
    cur->next = nullptr;
    list.size--;
    return cur;
}

bool rlDelete(ReservationList& list, int id) {
    Reservation* node = rlUnlink(list, id);
    if (node == nullptr) return false;
    delete node;
    return true;
}

void rlClear(ReservationList& list) {
    Reservation* cur = list.head;
    while (cur != nullptr) {
        Reservation* next = cur->next;
        delete cur;
        cur = next;
    }
    list.head = nullptr;
    list.tail = nullptr;
    list.size = 0;
}

const char* statusName(ResStatus s) {
    switch (s) {
        case ResStatus::Reserved: return "Reserved";
        case ResStatus::Ready: return "Ready";
        case ResStatus::Collected: return "Collected";
        case ResStatus::Cancelled: return "Cancelled";
        case ResStatus::Expired: return "Expired";
    }
    return "Unknown";
}

bool statusFromName(const std::string& name, ResStatus& out) {
    const ResStatus all[] = {ResStatus::Reserved, ResStatus::Ready, ResStatus::Collected,
                             ResStatus::Cancelled, ResStatus::Expired};
    for (ResStatus s : all) {
        if (name == statusName(s)) {
            out = s;
            return true;
        }
    }
    return false;
}

std::string resCode(int id) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "R-%04d", id);
    return buf;
}
