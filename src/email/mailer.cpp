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

bool rqRemove(RetryQueue& q, RetryItem* item) {
    RetryItem* prev = nullptr;
    for (RetryItem* cur = q.head; cur != nullptr; prev = cur, cur = cur->next) {
        if (cur != item) continue;
        if (prev == nullptr) q.head = cur->next;
        else prev->next = cur->next;
        if (q.tail == cur) q.tail = prev;
        cur->next = nullptr;
        q.count--;
        return true;
    }
    return false;
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
            it->attempts < 0 || it->attempts > RETRY_STEPS || !parseIntStrict(f[6], next)) {
            delete it;
            err = "pending_emails.txt line " + std::to_string(lineNo) + ": bad record";
            return false;
        }
        it->to = unescapeField(f[1]);
        it->subject = unescapeField(f[2]);
        it->body = unescapeField(f[3]);
        it->nextTryAt = (time_t)next;
        it->inFlight = false;
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

// Next try time after a failed attempt; false = give up. (it->attempts >= 1)
static bool reschedule(RetryItem* it, time_t now) {
    if (it->attempts > RETRY_STEPS) return false;
    it->nextTryAt = now + (time_t)RETRY_DELAY_MINUTES[it->attempts - 1] * 60;
    return true;
}

// True if the queue still holds mail for this person (call with m.mu held).
static bool waitingFor(const RetryQueue& q, const std::string& to) {
    for (RetryItem* it = q.head; it != nullptr; it = it->next) {
        if (it->to == to) return true;
    }
    return false;
}

static RetryItem* newItem(const Notification& n, int attempts, time_t nextTryAt) {
    RetryItem* it = new RetryItem;
    it->type = n.type;
    it->to = n.to;
    it->subject = n.subject;
    it->body = n.body;
    it->reservationId = n.reservationId;
    it->attempts = attempts;
    it->nextTryAt = nextTryAt;
    it->inFlight = false;
    it->next = nullptr;
    return it;
}

void mailerSendAll(Mailer& m, NotificationList& out) {
    if (out.count == 0) return;
    // Decide under the lock which emails must wait behind older mail.
    bool* behind = new bool[out.count];
    {
        std::lock_guard<std::mutex> lock(m.mu);
        int i = 0;
        for (Notification* n = out.head; n != nullptr; n = n->next) behind[i++] = waitingFor(m.retry, n->to);
    }
    // Send the others without holding it. A failure also makes later mail in
    // this batch for the same person wait behind it.
    RetryQueue later;  // in batch order: failures and mail held behind them
    rqInit(later);
    int i = 0;
    for (Notification* n = out.head; n != nullptr; n = n->next, i++) {
        if (behind[i] || waitingFor(later, n->to)) {
            rqEnqueue(later, newItem(*n, 0, 0));  // due at once, but behind the older mail
            continue;
        }
        if (!m.sender->send(*n)) rqEnqueue(later, newItem(*n, 1, 0));
    }
    delete[] behind;
    nlClear(out);
    if (later.count == 0) return;

    std::lock_guard<std::mutex> lock(m.mu);
    time_t now = m.clock->now();
    RetryItem* it;
    while ((it = rqDequeue(later)) != nullptr) {
        if (it->attempts == 0) it->nextTryAt = now;
        else reschedule(it, now);  // first failure: always schedulable
        rqEnqueue(m.retry, it);
    }
    savePending(m);
}

int mailerRetryDue(Mailer& m) {
    // 1. Under the lock, pick what to send: due, not already being sent, and
    //    every older item for the same person is picked too (keeps order).
    //    Picked items stay in the queue (and in the file) marked inFlight.
    RetryItem** picked = nullptr;
    int n = 0;
    time_t now;
    {
        std::lock_guard<std::mutex> lock(m.mu);
        now = m.clock->now();
        if (m.retry.count == 0) return 0;
        picked = new RetryItem*[m.retry.count];
        for (RetryItem* it = m.retry.head; it != nullptr; it = it->next) {
            bool olderBlocked = false;
            for (RetryItem* o = m.retry.head; o != it; o = o->next) {
                if (o->to == it->to) {
                    // an older item for this person blocks unless it is picked in this pass
                    bool oPicked = false;
                    for (int k = 0; k < n; k++) oPicked = oPicked || picked[k] == o;
                    if (!oPicked) olderBlocked = true;
                }
            }
            if (!it->inFlight && now >= it->nextTryAt && !olderBlocked) picked[n++] = it;
        }
        for (int k = 0; k < n; k++) picked[k]->inFlight = true;
    }

    // 2. Send without the lock, oldest first. If one fails, later picks for
    //    the same person are not tried in this pass.
    bool* sent = new bool[n > 0 ? n : 1];
    bool* tried = new bool[n > 0 ? n : 1];
    int delivered = 0;
    for (int k = 0; k < n; k++) {
        sent[k] = tried[k] = false;
        bool blocked = false;
        for (int j = 0; j < k; j++) {
            if (picked[j]->to == picked[k]->to && !sent[j]) blocked = true;
        }
        if (blocked) continue;
        tried[k] = true;
        sent[k] = m.sender->send(asNotification(picked[k]));
        if (sent[k]) delivered++;
    }

    // 3. Under the lock again: remove what was delivered or given up,
    //    reschedule failures in place, save.
    {
        std::lock_guard<std::mutex> lock(m.mu);
        for (int k = 0; k < n; k++) {
            RetryItem* it = picked[k];
            it->inFlight = false;
            if (!tried[k]) continue;
            it->attempts++;
            if (sent[k] || !reschedule(it, now)) {
                if (!sent[k]) {
                    logGivenUp(m, it);
                    std::fprintf(stderr, "EMAIL GIVEN UP after %d attempts: %s to %s\n", it->attempts,
                                 notifTypeName(it->type), it->to.c_str());
                }
                rqRemove(m.retry, it);
                delete it;
            }
        }
        savePending(m);
    }
    delete[] picked;
    delete[] sent;
    delete[] tried;
    return delivered;
}

int mailerWaiting(Mailer& m) {
    std::lock_guard<std::mutex> lock(m.mu);
    return m.retry.count;
}
