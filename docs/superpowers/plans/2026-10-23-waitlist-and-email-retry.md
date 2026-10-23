# Waitlist + Email Retry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Add suggested additions #1 (waitlist queue for sold-out fresh items) and #2 (email retry queue) to the existing system.

**Architecture:** Both are new hand-built queues. Waitlist: a circular array queue of `WaitEntry` values per fresh item, owned by `Cafeteria`, driven by the core rules (join / leave / promote / close at cutoff). Email retry: a linked-list queue of failed emails owned by a `Mailer` (email layer, its own mutex because emails are sent outside `app.mu`), retried by the minute timer.

**Tech Stack:** unchanged (C++17, hand-built structures, existing test harness, E2E driver).

**Spec:** `docs/suggested-additions.md` items 1–2; base system plan `2026-10-23-cafeteria-pre-order-system.md`.

## Global Constraints
- Same as the base plan: no STL containers in `src/ds|core|auth|storage|console`, **and now `src/email`** (added to the guard).
- Statuses stay exactly Reserved/Ready/Collected/Cancelled/Expired — waiting people are **not** reservations until promoted.
- Everything configurable stays configurable; no new hardcoded rule values except the retry schedule (1, 5, 15 min, then give up) documented as `[CONFIRM]`.

## Decisions made for this addition (override any of them)
- **W1 Auto-reserve.** When a portion frees up, the first person waiting gets a real reservation automatically (confirmation email says it came from the waitlist). No "offer and accept" step.
- **W2 Strict FIFO.** If the first person waits for 2 portions and only 1 is free, nobody behind them jumps ahead.
- **W3 When portions free up:** a cancellation before the deadline, staff raising the daily limit, or an undo that raises it. Promotion only happens before the cutoff.
- **W4 Joining:** only when the item cannot cover the requested quantity, before the cutoff, quantity within the per-person limit counting what the person already holds, one waitlist entry per person per item. Leaving is allowed any time before the cutoff.
- **W5 At the cutoff** everyone still waiting gets one "no portion became free today" email and the waitlist empties. At day rollover any leftovers are cleared silently.
- **W6 Leaving and skipped entries** stay in the circular queue marked inactive and are skipped at the front (same lazy pattern the proposal uses for cancelled cooking entries).
- **W7 Capacity** = the cooking queue capacity per item; join refused with a clear message when full.
- **E1 Retry schedule:** 1 min, 5 min, 15 min after the failure, then give up and append it to `data/email_failed.log` `[CONFIRM]`.
- **E2 Order:** the retry queue is FIFO; each timer tick walks it once (dequeue, send if due, else re-enqueue at the rear) because items have different due times.
- **E3 Persistence:** waiting emails are saved to `data/pending_emails.txt` so a restart does not lose them.

## Review Focus
1. A person who cancels and a waiting person both act within the same minute → promotion uses the freshly returned portion exactly once (no double booking).
2. Promoted person already reached the per-person limit since joining → skipped (not over-booked), next person considered.
3. Restart with people waiting → order preserved, nobody duplicated.
4. SMTP down for a while → emails go out later in the original order; none lost on restart; none sent twice.
5. Retry queue never touched under `app.mu` and the mailer mutex in opposite orders (no deadlock).

---

### Task A1: Waitlist queue (`src/ds/wait_queue.*`)
Produces: `struct WaitEntry { std::string user; int quantity; time_t joinedAt; bool active; };`
`struct WaitQueue { WaitEntry* slots; int capacity, front, rear, count; };`
`bool wqInit(WaitQueue&, int); void wqFree(WaitQueue&); bool wqIsEmpty/wqIsFull(const WaitQueue&); bool wqEnqueue(WaitQueue&, const WaitEntry&); bool wqDequeue(WaitQueue&, WaitEntry& out); WaitEntry* wqPeek(WaitQueue&); WaitEntry* wqAt(WaitQueue&, int i); void wqClear(WaitQueue&);`
- [ ] Tests: empty dequeue/peek, FIFO, full refuses, wraparound, `wqAt` from front. RED → implement → GREEN → commit.

### Task A2: Waitlist rules (`src/core/waitlist.cpp`, declarations in `cafeteria.h`)
Produces: `WaitQueue waitlists[MAX_MENU_ITEMS]` in `Cafeteria`;
`Result joinWaitlist(Cafeteria&, const std::string& user, int item, int qty, int& position);` (new Results: `NotSoldOut`, `AlreadyWaiting`, `WaitlistFull`, `NotWaiting`)
`Result leaveWaitlist(Cafeteria&, const std::string& user, int item);`
`int waitlistPosition(const Cafeteria&, const std::string& user, int item);` (1-based, 0 = not waiting)
`int waitlistActiveCount(const Cafeteria&, int item);`
`int promoteWaitlist(Cafeteria&, int item, NotificationList& out);` (called by cancel / setDailyLimit / undo)
Signature changes: `cancelReservation(..., NotificationList& out)`, `setDailyLimit(..., NotificationList& out, bool&)`, `undoLast(..., NotificationList& out, StaffAction&)`.
`timerTick` closes waitlists at cutoff (W5); rollover clears them.
- [ ] Tests: `join_only_when_sold_out`, `join_twice_refused`, `join_counts_portions_already_held`, `join_after_cutoff_refused`, `cancel_promotes_first_waiting_with_email`, `promotion_is_strict_fifo`, `left_entries_are_skipped`, `promotion_skips_person_now_at_limit`, `limit_raise_promotes`, `undo_that_raises_limit_promotes`, `no_promotion_after_cutoff`, `cutoff_closes_waitlist_with_one_email_each`, `rollover_clears_waitlist`, `waitlist_full_refused`, `cancel_and_promote_use_portion_once`.

### Task A3: Waitlist persistence (`storage.cpp`)
`waitlist.txt`: `day|item|user|qty|joinedAt` for active entries in queue order; loaded into today's queues only.
- [ ] Tests: `waitlist_order_survives_restart`, `old_day_waitlist_not_loaded`, `left_entries_not_saved`.

### Task B1: Email retry queue (`src/email/mailer.*`)
Produces: `struct RetryItem { NotifType type; std::string to, subject, body; int reservationId; int attempts; time_t nextTryAt; RetryItem* next; };`
`struct RetryQueue { RetryItem* head; RetryItem* tail; int count; };` + `rqInit/rqEnqueue/rqDequeue/rqClear`
`struct Mailer { EmailSender* sender; Clock* clock; std::string dataDir; RetryQueue retry; std::mutex mu; int givenUp; };`
`void mailerInit(Mailer&, EmailSender*, Clock*, const std::string& dataDir); void mailerFree(Mailer&);`
`void mailerSendAll(Mailer&, NotificationList& out);` (send now; failures → retry queue, attempts=1, due +1 min)
`int mailerRetryDue(Mailer&);` (one pass; returns sent count; gives up after 3 retries)
`int mailerWaiting(Mailer&);` `bool mailerLoad(Mailer&, std::string& err);` (pending_emails.txt, escaped fields)
- [ ] Tests (fake sender that fails N times): `failed_email_is_queued_not_lost`, `retried_after_one_minute`, `schedule_1_5_15_then_gives_up_and_logs`, `retry_keeps_original_order`, `not_due_items_wait`, `pending_emails_survive_restart`, `body_with_newlines_and_pipes_round_trips`.

### Task C: Server, pages, E2E
- Routes: `POST /api/waitlist/join` (item, quantity), `POST /api/waitlist/leave` (item); `/api/menu` items get `waiting` (active count) and `yourPosition`; staff overview items get `waitlist`; overview gets `emailsWaiting`.
- `sendAll` → `mailerSendAll`; timer thread and `/api/dev/advance` call `mailerRetryDue` (outside `app.mu`).
- Home page: sold-out card offers "Join waitlist"; waiting card shows "You are #N" + "Leave waitlist". Staff portal shows waitlist count and emails waiting.
- E2E additions: `waitlist_join_cancel_promotes_with_email`, `waitlist_closes_at_cutoff_with_email`, `waitlist_survives_restart`, `email_failure_is_retried_after_outbox_recovers` (make `outbox` a file to force failures, then restore it and advance 1 minute).
- [ ] Final: fresh reviewer, fix pass, clean rebuild, all suites.
