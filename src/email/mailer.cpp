#include "email/mailer.h"
#include "storage/file_util.h"
#include <cstdio>
#include <fstream>

const int RETRY_DELAY_MINUTES[RETRY_STEPS] = {1, 5, 15};

// ---- the linked queue ------------------------------------------------------------

void rqInit(RetryQueue& q) {
    q.head = nullptr;
    q.tail = nullptr;
    q.count = 0;
}

void rqEnqueue(RetryQueue& q, RetryItem* item) {
    item->next = nullptr;
    if (q.tail == nullptr) q.head = item;
    else q.tail->next = item;
    q.tail = item;
    q.count++;
}

RetryItem* rqDequeue(RetryQueue& q) {
    RetryItem* item = q.head;
    if (item == nullptr) return nullptr;
    q.head = item->next;
    if (q.head == nullptr) q.tail = nullptr;
    item->next = nullptr;
    q.count--;
    return item;
}

void rqClear(RetryQueue& q) {
    RetryItem* item;
    while ((item = rqDequeue(q)) != nullptr) delete item;
}

// ---- file format: one email per line, fields escaped ----------------------------------

static std::string escapeField(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '|') out += "\\p";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

static std::string unescapeField(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] != '\\' || i + 1 == s.size()) {
            out += s[i];
            continue;
        }
        char n = s[++i];
        out += n == 'p' ? '|' : n == 'n' ? '\n' : n == 'r' ? '\r' : n;
    }
    return out;
}

static const NotifType ALL_TYPES[] = {NotifType::Confirmation, NotifType::Ready,
                                      NotifType::Reminder, NotifType::Expired,
                                      NotifType::VerificationCode, NotifType::WaitlistClosed};

static bool typeFromName(const std::string& name, NotifType& out) {
    for (NotifType t : ALL_TYPES) {
        if (name == notifTypeName(t)) {
            out = t;
            return true;
        }
    }
    return false;
}

// Call with m.mu held.
static void savePending(Mailer& m) {
    std::string s = "# type|to|subject|body|reservationId|attempts|nextTryAt (escaped)\n";
    for (RetryItem* it = m.retry.head; it != nullptr; it = it->next) {
        s += std::string(notifTypeName(it->type)) + "|" + escapeField(it->to) + "|" + escapeField(it->subject) +
             "|" + escapeField(it->body) + "|" + std::to_string(it->reservationId) + "|" +
             std::to_string(it->attempts) + "|" + std::to_string((long long)it->nextTryAt) + "\n";
    }
    std::string err;
    if (!atomicWrite(m.dataDir + "/pending_emails.txt", s, err))
        std::fprintf(stderr, "Cannot save pending emails: %s\n", err.c_str());
}

void mailerInit(Mailer& m, EmailSender* sender, Clock* clock, const std::string& dataDir) {
    m.sender = sender;
    m.clock = clock;
    m.dataDir = dataDir;
    rqInit(m.retry);
    m.givenUp = 0;
}

void mailerFree(Mailer& m) {
    std::lock_guard<std::mutex> lock(m.mu);
    rqClear(m.retry);
}

bool mailerLoad(Mailer& m, std::string& err) {
    std::lock_guard<std::mutex> lock(m.mu);
    std::ifstream in(m.dataDir + "/pending_emails.txt");
    if (!in) return true;
    std::string line, f[7];
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        RetryItem* it = new RetryItem;
        long long next;
        if (splitFields(line, '|', f, 7) != 7 || !typeFromName(f[0], it->type) ||
            !parseIntStrict(f[4], it->reservationId) || !parseIntStrict(f[5], it->attempts) ||
            !parseIntStrict(f[6], next)) {
            delete it;
            err = "pending_emails.txt line " + std::to_string(lineNo) + ": bad record";
            return false;
        }
        it->to = unescapeField(f[1]);
        it->subject = unescapeField(f[2]);
        it->body = unescapeField(f[3]);
        it->nextTryAt = (time_t)next;
        rqEnqueue(m.retry, it);
    }
    return true;
}

static Notification asNotification(const RetryItem* it) {
    Notification n;
    n.type = it->type;
    n.to = it->to;
    n.subject = it->subject;
    n.body = it->body;
    n.reservationId = it->reservationId;
    n.next = nullptr;
    return n;
}

static void logGivenUp(Mailer& m, const RetryItem* it) {
    std::string err;
    appendFile(m.dataDir + "/email_failed.log",
               std::to_string((long long)m.clock->now()) + "|" + notifTypeName(it->type) + "|" + it->to + "|" +
                   escapeField(it->subject) + "\n",
               err);
    m.givenUp++;
}

// After a failed attempt: schedule the next try, or give up. Call with m.mu held.
static void afterFailure(Mailer& m, RetryItem* it, time_t now) {
    if (it->attempts > RETRY_STEPS) {
        logGivenUp(m, it);
        std::fprintf(stderr, "EMAIL GIVEN UP after %d attempts: %s to %s\n", it->attempts,
                     notifTypeName(it->type), it->to.c_str());
        delete it;
        return;
    }
    it->nextTryAt = now + (time_t)RETRY_DELAY_MINUTES[it->attempts - 1] * 60;
    rqEnqueue(m.retry, it);
}

void mailerSendAll(Mailer& m, NotificationList& out) {
    RetryQueue failed;
    rqInit(failed);
    for (Notification* n = out.head; n != nullptr; n = n->next) {
        if (m.sender->send(*n)) continue;
        RetryItem* it = new RetryItem;
        it->type = n->type;
        it->to = n->to;
        it->subject = n->subject;
        it->body = n->body;
        it->reservationId = n->reservationId;
        it->attempts = 1;
        rqEnqueue(failed, it);
    }
    nlClear(out);
    if (failed.count == 0) return;

    std::lock_guard<std::mutex> lock(m.mu);
    time_t now = m.clock->now();
    RetryItem* it;
    while ((it = rqDequeue(failed)) != nullptr) afterFailure(m, it, now);
    savePending(m);
}

int mailerRetryDue(Mailer& m) {
    // Take the due items out under the lock, keeping the others in order...
    RetryQueue due;
    rqInit(due);
    time_t now;
    {
        std::lock_guard<std::mutex> lock(m.mu);
        now = m.clock->now();
        int n = m.retry.count;
        for (int i = 0; i < n; i++) {  // one full pass: due items out, the rest back to the rear
            RetryItem* it = rqDequeue(m.retry);
            if (now >= it->nextTryAt) rqEnqueue(due, it);
            else rqEnqueue(m.retry, it);
        }
    }
    if (due.count == 0) return 0;

    // ...send them without holding it...
    int delivered = 0;
    RetryQueue failed;
    rqInit(failed);
    RetryItem* it;
    while ((it = rqDequeue(due)) != nullptr) {
        it->attempts++;
        if (m.sender->send(asNotification(it))) {
            delivered++;
            delete it;
        } else {
            rqEnqueue(failed, it);
        }
    }

    // ...then reschedule the failures and save.
    std::lock_guard<std::mutex> lock(m.mu);
    while ((it = rqDequeue(failed)) != nullptr) afterFailure(m, it, now);
    savePending(m);
    return delivered;
}

int mailerWaiting(Mailer& m) {
    std::lock_guard<std::mutex> lock(m.mu);
    return m.retry.count;
}
