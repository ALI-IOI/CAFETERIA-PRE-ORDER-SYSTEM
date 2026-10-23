// Emails the rules want sent. Core functions only append to this list; the
// server sends them after releasing the mutex (ADR-001).
#pragma once
#include <string>

enum class NotifType { Confirmation, Ready, Reminder, Expired, VerificationCode };

struct Notification {
    NotifType type;
    std::string to;
    std::string subject;
    std::string body;
    int reservationId;  // 0 when not about a reservation
    Notification* next;
};

// Singly linked list with a tail pointer: append O(1), read front to back.
struct NotificationList {
    Notification* head;
    Notification* tail;
    int count;
};

void nlInit(NotificationList& list);
void nlAppend(NotificationList& list, NotifType type, const std::string& to,
              const std::string& subject, const std::string& body, int reservationId);
void nlClear(NotificationList& list);
const char* notifTypeName(NotifType t);
