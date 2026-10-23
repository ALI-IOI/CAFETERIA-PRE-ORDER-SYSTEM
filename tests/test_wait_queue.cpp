#include "testing.h"
#include "ds/wait_queue.h"

static WaitEntry entry(const char* user, int qty) {
    WaitEntry e;
    e.user = user;
    e.quantity = qty;
    e.joinedAt = 0;
    e.active = true;
    return e;
}

TEST(waitq_empty_dequeue_and_peek) {
    WaitQueue q;
    CHECK(wqInit(q, 3));
    CHECK(wqIsEmpty(q));
    WaitEntry out;
    CHECK(!wqDequeue(q, out));
    CHECK(wqPeek(q) == nullptr);
    wqFree(q);
}

TEST(waitq_fifo_order) {
    WaitQueue q;
    wqInit(q, 4);
    wqEnqueue(q, entry("a", 1));
    wqEnqueue(q, entry("b", 2));
    wqEnqueue(q, entry("c", 1));
    WaitEntry out;
    CHECK(wqDequeue(q, out));
    CHECK_EQ(out.user, std::string("a"));
    CHECK(wqDequeue(q, out));
    CHECK_EQ(out.user, std::string("b"));
    CHECK_EQ(out.quantity, 2);
    CHECK_EQ(wqPeek(q)->user, std::string("c"));
    wqFree(q);
}

TEST(waitq_full_refuses_and_keeps_contents) {
    WaitQueue q;
    wqInit(q, 2);
    CHECK(wqEnqueue(q, entry("a", 1)));
    CHECK(wqEnqueue(q, entry("b", 1)));
    CHECK(wqIsFull(q));
    CHECK(!wqEnqueue(q, entry("c", 1)));
    CHECK_EQ(q.count, 2);
    CHECK_EQ(wqAt(q, 1)->user, std::string("b"));
    wqFree(q);
}

TEST(waitq_wraparound_and_at_from_front) {
    WaitQueue q;
    wqInit(q, 3);
    wqEnqueue(q, entry("a", 1));
    wqEnqueue(q, entry("b", 1));
    wqEnqueue(q, entry("c", 1));
    WaitEntry out;
    wqDequeue(q, out);
    wqDequeue(q, out);
    CHECK(wqEnqueue(q, entry("d", 1)));  // wraps
    CHECK(wqEnqueue(q, entry("e", 1)));
    CHECK_EQ(wqAt(q, 0)->user, std::string("c"));
    CHECK_EQ(wqAt(q, 1)->user, std::string("d"));
    CHECK_EQ(wqAt(q, 2)->user, std::string("e"));
    CHECK(wqAt(q, 3) == nullptr);
    wqAt(q, 1)->active = false;  // entries can be marked in place (lazy leave)
    wqDequeue(q, out);
    wqDequeue(q, out);
    CHECK(!out.active);
    wqFree(q);
}

TEST(waitq_clear_and_bad_capacity) {
    WaitQueue q;
    CHECK(!wqInit(q, 0));
    wqInit(q, 2);
    wqEnqueue(q, entry("a", 1));
    wqClear(q);
    CHECK(wqIsEmpty(q));
    CHECK(wqEnqueue(q, entry("b", 1)));
    wqFree(q);
}
