#include "testing.h"
#include "ds/res_queue.h"

// Queue entries are pointers to reservation nodes; plain stack objects are
// enough to test the queue itself.
static Reservation makeRes(int id) {
    Reservation r;
    r.id = id;
    r.next = nullptr;
    return r;
}

TEST(queue_dequeue_empty_returns_null) {
    ResQueue q;
    CHECK(qInit(q, 3));
    CHECK(qIsEmpty(q));
    CHECK(qDequeue(q) == nullptr);
    qFree(q);
}

TEST(queue_peek_empty_returns_null) {
    ResQueue q;
    qInit(q, 3);
    CHECK(qPeek(q) == nullptr);
    qFree(q);
}

TEST(queue_fifo_order) {
    Reservation a = makeRes(1), b = makeRes(2), c = makeRes(3);
    ResQueue q;
    qInit(q, 5);
    qEnqueue(q, &a);
    qEnqueue(q, &b);
    qEnqueue(q, &c);
    CHECK(qPeek(q) == &a);
    CHECK(qDequeue(q) == &a);
    CHECK(qDequeue(q) == &b);
    CHECK(qDequeue(q) == &c);
    CHECK(qIsEmpty(q));
    qFree(q);
}

TEST(queue_enqueue_full_returns_false_and_keeps_contents) {
    Reservation a = makeRes(1), b = makeRes(2), c = makeRes(3);
    ResQueue q;
    qInit(q, 2);
    CHECK(qEnqueue(q, &a));
    CHECK(qEnqueue(q, &b));
    CHECK(qIsFull(q));
    CHECK(!qEnqueue(q, &c));
    CHECK_EQ(q.count, 2);
    CHECK(qDequeue(q) == &a);
    CHECK(qDequeue(q) == &b);
    qFree(q);
}

TEST(queue_wraparound_after_dequeues) {
    Reservation r[5] = {makeRes(1), makeRes(2), makeRes(3), makeRes(4), makeRes(5)};
    ResQueue q;
    qInit(q, 3);
    qEnqueue(q, &r[0]);
    qEnqueue(q, &r[1]);
    qEnqueue(q, &r[2]);
    qDequeue(q);
    qDequeue(q);
    CHECK(qEnqueue(q, &r[3]));  // these two wrap around the end of the array
    CHECK(qEnqueue(q, &r[4]));
    CHECK_EQ(q.count, 3);
    CHECK(qIsFull(q));
    CHECK_EQ(qDequeue(q)->id, 3);
    CHECK_EQ(qDequeue(q)->id, 4);
    CHECK_EQ(qDequeue(q)->id, 5);
    CHECK(qIsEmpty(q));
    qFree(q);
}

TEST(queue_init_rejects_nonpositive_capacity) {
    ResQueue q;
    CHECK(!qInit(q, 0));
    CHECK(!qInit(q, -1));
}

TEST(queue_at_reads_front_to_rear_for_display) {
    Reservation a = makeRes(1), b = makeRes(2), c = makeRes(3);
    ResQueue q;
    qInit(q, 2);
    qEnqueue(q, &a);
    qEnqueue(q, &b);
    qDequeue(q);
    qEnqueue(q, &c);  // wraps
    CHECK(qAt(q, 0) == &b);
    CHECK(qAt(q, 1) == &c);
    CHECK(qAt(q, 2) == nullptr);
    qFree(q);
}
