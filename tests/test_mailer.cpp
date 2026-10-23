#include "testing.h"
#include "core/clock.h"
#include "email/mailer.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Sender that fails the next `failNext` sends (or always, if failAlways).
struct FlakySender : EmailSender {
    int failNext = 0;
    bool failAlways = false;
    int attempts = 0;
    std::string delivered;  // "to," in delivery order
    bool send(const Notification& n) override {
        attempts++;
        if (failAlways) return false;
        if (failNext > 0) {
            failNext--;
            return false;
        }
        delivered += n.to + ",";
        return true;
    }
    const char* modeName() const override { return "fake"; }
};

static std::string freshDir(const char* name) {
    fs::path p = fs::temp_directory_path() / "cafeteria_tests" / name;
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

struct MailFixture {
    FakeClock clock;
    FlakySender sender;
    Mailer mailer;
    NotificationList out;
    std::string dir;
    explicit MailFixture(const char* name) : dir(freshDir(name)) {
        clock.set(2026, 10, 23, 10, 0, 0);
        mailerInit(mailer, &sender, &clock, dir);
        nlInit(out);
    }
    ~MailFixture() {
        nlClear(out);
        mailerFree(mailer);
    }
    void queueMail(const char* to, const char* body = "b") {
        nlAppend(out, NotifType::Ready, to, "Ready", body, 7);
    }
};

TEST(failed_email_is_queued_not_lost) {
    MailFixture f("mail_queue");
    f.sender.failNext = 1;
    f.queueMail("a@uni.example");
    f.queueMail("b@uni.example");
    mailerSendAll(f.mailer, f.out);
    CHECK_EQ(f.out.count, 0);  // list consumed
    CHECK_EQ(f.sender.delivered, std::string("b@uni.example,"));
    CHECK_EQ(mailerWaiting(f.mailer), 1);
}

TEST(retried_after_one_minute) {
    MailFixture f("mail_retry");
    f.sender.failNext = 1;
    f.queueMail("a@uni.example");
    mailerSendAll(f.mailer, f.out);
    f.clock.t += 59;
    CHECK_EQ(mailerRetryDue(f.mailer), 0);
    CHECK_EQ(f.sender.attempts, 1);  // not tried again early
    f.clock.t += 1;
    CHECK_EQ(mailerRetryDue(f.mailer), 1);
    CHECK_EQ(f.sender.delivered, std::string("a@uni.example,"));
    CHECK_EQ(mailerWaiting(f.mailer), 0);
}

TEST(schedule_1_5_15_then_gives_up_and_logs) {
    MailFixture f("mail_giveup");
    f.sender.failAlways = true;
    f.queueMail("a@uni.example");
    mailerSendAll(f.mailer, f.out);           // attempt 1 at 10:00
    int expectAttemptsAt[][2] = {{1, 2}, {6, 3}, {21, 4}};  // minute after 10:00, total attempts
    for (auto& step : expectAttemptsAt) {
        f.clock.set(2026, 10, 23, 10, step[0] - 1, 59);
        mailerRetryDue(f.mailer);
        CHECK_EQ(f.sender.attempts, step[1] - 1);  // a second early: nothing
        f.clock.set(2026, 10, 23, 10, step[0], 0);
        mailerRetryDue(f.mailer);
        CHECK_EQ(f.sender.attempts, step[1]);
    }
    CHECK_EQ(mailerWaiting(f.mailer), 0);  // gave up after the 15-minute retry
    CHECK_EQ(f.mailer.givenUp, 1);
    std::ifstream log(f.dir + "/email_failed.log");
    std::string all((std::istreambuf_iterator<char>(log)), std::istreambuf_iterator<char>());
    CHECK(all.find("a@uni.example") != std::string::npos);
    f.clock.set(2026, 10, 23, 12, 0, 0);
    mailerRetryDue(f.mailer);
    CHECK_EQ(f.sender.attempts, 4);  // never tried again
}

TEST(retry_keeps_original_order) {
    MailFixture f("mail_order");
    f.sender.failNext = 3;
    f.queueMail("a@uni.example");
    f.queueMail("b@uni.example");
    f.queueMail("c@uni.example");
    mailerSendAll(f.mailer, f.out);
    f.clock.t += 60;
    CHECK_EQ(mailerRetryDue(f.mailer), 3);
    CHECK_EQ(f.sender.delivered, std::string("a@uni.example,b@uni.example,c@uni.example,"));
}

TEST(not_due_items_wait) {
    MailFixture f("mail_due");
    f.sender.failNext = 1;
    f.queueMail("a@uni.example");
    mailerSendAll(f.mailer, f.out);  // a fails at 10:00:00
    f.clock.t += 30;
    f.sender.failNext = 1;
    f.queueMail("b@uni.example");
    mailerSendAll(f.mailer, f.out);  // b fails at 10:00:30
    f.clock.t += 30;                 // 10:01:00
    CHECK_EQ(mailerRetryDue(f.mailer), 1);
    CHECK_EQ(f.sender.delivered, std::string("a@uni.example,"));
    CHECK_EQ(mailerWaiting(f.mailer), 1);
    f.clock.t += 30;                 // 10:01:30
    CHECK_EQ(mailerRetryDue(f.mailer), 1);
    CHECK_EQ(f.sender.delivered, std::string("a@uni.example,b@uni.example,"));
}

TEST(pending_emails_survive_restart) {
    std::string dir;
    time_t failedAt;
    {
        MailFixture f("mail_restart");
        dir = f.dir;
        f.sender.failNext = 1;
        f.queueMail("a@uni.example", "Your Tehari (R-0001) is ready.");
        mailerSendAll(f.mailer, f.out);
        failedAt = f.clock.now();
    }
    FakeClock clock;
    clock.t = failedAt + 120;
    FlakySender sender;
    Mailer m;
    mailerInit(m, &sender, &clock, dir);
    std::string err;
    CHECK(mailerLoad(m, err));
    CHECK_EQ(mailerWaiting(m), 1);
    CHECK_EQ(mailerRetryDue(m), 1);
    CHECK_EQ(sender.delivered, std::string("a@uni.example,"));
    CHECK_EQ(mailerWaiting(m), 0);
    mailerFree(m);

    Mailer again;  // the delivered email is not sent a second time after another restart
    mailerInit(again, &sender, &clock, dir);
    CHECK(mailerLoad(again, err));
    CHECK_EQ(mailerWaiting(again), 0);
    mailerFree(again);
}

TEST(body_with_newlines_and_pipes_round_trips) {
    std::string dir;
    std::string body = "Line one | with pipe\nLine two \\ backslash\r\nend";
    {
        MailFixture f("mail_escape");
        dir = f.dir;
        f.sender.failNext = 1;
        nlAppend(f.out, NotifType::Reminder, "a@uni.example", "Subject | x", body, 3);
        mailerSendAll(f.mailer, f.out);
    }
    FakeClock clock;
    clock.set(2026, 10, 23, 11, 0, 0);
    struct Capture : EmailSender {
        Notification got;
        bool send(const Notification& n) override {
            got = n;
            return true;
        }
        const char* modeName() const override { return "capture"; }
    } cap;
    Mailer m;
    mailerInit(m, &cap, &clock, dir);
    std::string err;
    CHECK(mailerLoad(m, err));
    CHECK_EQ(mailerRetryDue(m), 1);
    CHECK_EQ(cap.got.body, body);
    CHECK_EQ(cap.got.subject, std::string("Subject | x"));
    CHECK_EQ(cap.got.type, NotifType::Reminder);
    CHECK_EQ(cap.got.reservationId, 3);
    mailerFree(m);
}
