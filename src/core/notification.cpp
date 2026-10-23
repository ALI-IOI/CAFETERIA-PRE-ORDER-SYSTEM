#include "core/notification.h"

void nlInit(NotificationList& list) {
    list.head = nullptr;
    list.tail = nullptr;
    list.count = 0;
}

void nlAppend(NotificationList& list, NotifType type, const std::string& to,
              const std::string& subject, const std::string& body, int reservationId) {
    Notification* n = new Notification;
    n->type = type;
    n->to = to;
    n->subject = subject;
    n->body = body;
    n->reservationId = reservationId;
    n->next = nullptr;
    if (list.tail == nullptr) list.head = n;
    else list.tail->next = n;
    list.tail = n;
    list.count++;
}

void nlClear(NotificationList& list) {
    Notification* cur = list.head;
    while (cur != nullptr) {
        Notification* next = cur->next;
        delete cur;
        cur = next;
    }
    nlInit(list);
}

const char* notifTypeName(NotifType t) {
    switch (t) {
        case NotifType::Confirmation: return "confirmation";
        case NotifType::Ready: return "ready";
        case NotifType::Reminder: return "reminder";
        case NotifType::Expired: return "expired";
        case NotifType::VerificationCode: return "verification";
        case NotifType::WaitlistClosed: return "waitlist-closed";
    }
    return "unknown";
}
