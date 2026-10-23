// Email delivery with retries (suggested addition #2).
//
// Every email is tried once straight away. If it fails it joins the retry
// queue — a hand-built linked-list FIFO — and the minute timer tries again
// 1, 5 and 15 minutes after each failure [CONFIRM schedule]. After the last
// failure it is written to data/email_failed.log and dropped. The queue is
// saved to data/pending_emails.txt after every change so a restart loses nothing.
//
// Locking: the mailer has its own mutex and never takes the server's app.mu,
// and sending happens outside the mailer mutex (SMTP can be slow).
#pragma once
#include "core/clock.h"
#include "core/notification.h"
#include "email/email.h"
#include <ctime>
#include <mutex>
#include <string>

struct RetryItem {
    NotifType type;
    std::string to, subject, body;
    int reservationId;
    int attempts;      // sends tried so far
    time_t nextTryAt;
    RetryItem* next;
};

// Linked FIFO queue: enqueue at the tail, dequeue at the head, both O(1).
struct RetryQueue {
    RetryItem* head;
    RetryItem* tail;
    int count;
};

void rqInit(RetryQueue& q);
void rqEnqueue(RetryQueue& q, RetryItem* item);
RetryItem* rqDequeue(RetryQueue& q);  // nullptr when empty; caller owns the item
void rqClear(RetryQueue& q);

const int RETRY_STEPS = 3;
extern const int RETRY_DELAY_MINUTES[RETRY_STEPS];  // {1, 5, 15}

struct Mailer {
    EmailSender* sender;
    Clock* clock;
    std::string dataDir;
    RetryQueue retry;
    std::mutex mu;
    int givenUp;  // emails dropped after the last retry (since start)
};

void mailerInit(Mailer& m, EmailSender* sender, Clock* clock, const std::string& dataDir);
void mailerFree(Mailer& m);
bool mailerLoad(Mailer& m, std::string& err);  // reads pending_emails.txt (missing file = empty)

// Sends every notification now; failures go to the retry queue. Empties `out`.
void mailerSendAll(Mailer& m, NotificationList& out);

// One pass over the queue: due items are sent (or rescheduled / given up),
// others keep their place. Returns how many were delivered.
int mailerRetryDue(Mailer& m);

int mailerWaiting(Mailer& m);
