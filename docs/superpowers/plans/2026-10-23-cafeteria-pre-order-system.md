# University Cafeteria Food Pre-Order System — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the BAUSTK cafeteria pre-order system from the DSA I proposal: a C++ server whose reservation logic runs on hand-built array, linked-list, circular-queue and stack structures, with student/teacher pages, a staff portal, email notifications and file persistence.

**Architecture:** See `docs/architecture/ADR-001-server-architecture.md`. Layers: `ds/` (four structures, no STL containers) → `core/` (rules, returns notifications) → `storage/` (text files) → `auth/` → `email/` → `server/` (cpp-httplib, one global mutex, minute timer thread) → `web/` (static HTML/CSS/JS).

**Tech Stack:** C++17, MinGW-w64 g++ 13.1, CMake + Ninja, cpp-httplib (header), libsodium (or Windows CNG — D1), Windows `curl.exe` for SMTP, HTML/CSS/vanilla JS.

**Spec:** `Cafeteria_Pre_Order_System_DSA_Proposal.docx` (copied to `docs/proposal/` in Task 1).

## Global Constraints

- Array, linked lists, queues and stack are written with structs, pointers, arrays and free functions. No `std::vector`, `std::list`, `std::queue`, `std::stack`, `std::deque`, `std::map`, `std::set`, `std::unordered_*` anywhere in `src/ds/`, `src/core/`, `src/auth/`, `src/storage/` (§7). `std::string` is allowed.
- Third-party code only for web, email and security (§7, §11).
- Staff-changeable values are data, not constants (§4): cutoff, cancellation deadline, freshness window, reminder interval, per-user limit, per-item daily limit.
- Statuses exactly: Reserved, Ready, Collected, Cancelled, Expired (§4).
- Packaged items are view-only; only Fresh items are reservable (§4).
- Out of scope: payment, delivery, mobile app, advanced analytics (§4).
- Every unconfirmed value is written as a placeholder and listed at server start-up as `[CONFIRM] ...` (see "Placeholders" below).
- Build: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`.

## Open decisions — need your answer before Task 1 (my recommendation first)

- **D1 Libraries / downloads.** No libsodium/OpenSSL/libcurl headers are installed. Recommended: download **cpp-httplib** (`httplib.h`, github.com/yhirose/cpp-httplib, ~0.5 MB, tag v0.18.x) and the official **libsodium MinGW package** (download.libsodium.org, `libsodium-1.0.20-mingw.tar.gz`, ~4 MB) into `third_party/`; send SMTP by running Windows' own `curl.exe` (it is libcurl). Alternative: skip libsodium and use Windows CNG (PBKDF2-SHA256 + `BCryptGenRandom`), which needs no download but is Windows-only and not literally "libsodium or OpenSSL". HTTPS: document a reverse proxy for deployment; local demo is HTTP (proposal §14 allows simplifying HTTPS first).
- **D2 Mark ready = whole queue or a batch?** §8 says staff "mark an item ready" and the server dequeues FIFO. Recommended: staff enter how many portions are cooked (default = all waiting); the server dequeues FIFO until that many portions are covered, skipping Cancelled. This makes FIFO visible in the demo.
- **D3 Mark collected is per reservation** (staff search R-0042, mark it collected). §5 is worded per item; per-reservation is the only reading that stops one person's reminders. Recommended: per reservation.
- **D4 What the undo stack holds.** Recommended: Set stock, Set daily limit, Change a setting, Mark collected. **Mark ready is not pushed** (its emails cannot be unsent); it is logged but not undoable. Undo of a limit change that would drop below portions already reserved is refused.
- **D5 Stack full.** Fixed capacity `[CONFIRM: 50]`. Recommended: the staff action still happens, the push fails, and the portal shows "Undo history full — this action cannot be undone." (Refusing the action would block staff work.)
- **D6 Queue full.** Cooking queue capacity per item `[CONFIRM: 100]` (cancelled entries stay in the queue, so reservations can exceed the daily limit). Recommended: a reservation whose cooking queue is full is rejected with "Too many reservations for this item today". Reminder queue capacity = sum of cooking capacities, so it can only fill in unit tests.
- **D7 Reminder queue ordering — proposal inconsistency.** §8 says the front is always the earliest due "because every reminder is due one hour after the previous one". That holds only while the interval is constant and the freshness window is a whole multiple of it. If staff change the interval mid-day, or the window is e.g. 7.5 h, a reminder or expiry can be delayed by up to one interval behind the front. Recommended: keep the proposal's front-only check (it is the point of the queue); new interval/window values apply only to items marked ready after the change; document the edge case. Collected entries leave the queue lazily (skipped at the front), same as cancelled ones in the cooking queue.
- **D8 Cutoff boundary.** Recommended: reserving/cancelling is allowed while `now < cutoff`; at exactly 10:00:00 it is refused.
- **D9 Day rollover** (not covered by the proposal). Recommended: on the first timer tick of a new date, Reserved reservations never cooked become Expired (no email), each item's reserved count resets to 0, cooking queues are emptied, and previous-day final-state reservations are appended to `data/history.txt` and deleted from the list.
- **D10 Staff account creation.** "Created by an administrator" (§5) — there is no admin role. Recommended: a console tool `cafeteria_admin add-staff <login> <name>` (prompts for password) run on the server machine; no web route can create staff.
- **D11 Git.** The folder is not a git repository. Recommended: `git init` in Task 1 and one commit per task (local only, never pushed).

## Placeholders (shipped as `[CONFIRM]`, printed at start-up)

| Item | Placeholder value used so the demo runs | Source |
|---|---|---|
| Cutoff / cancellation deadline | `[CONFIRM: 10:00]` / `[CONFIRM: 10:00]` | §4 (assumption) |
| Freshness window | `[CONFIRM: 7 h]` | §4 (cafeteria says 7–8 h) |
| Reminder interval | `[CONFIRM: 1 h]` | §4 |
| Per-user limit | `[CONFIRM: 2 per item]` | §4 |
| Daily limits Khichuri / Fried Rice / Tehari | `[CONFIRM: 12 / 8 / 10]` | §9 sample, "indicative only" |
| Packaged list | Chips: Alloz, Mr. Twist, Lays, Sun; Soft drinks: Dew, Pepsi, Mirinda, 7UP, Coca-Cola; `[CONFIRM: water brand(s)]`; `[CONFIRM: biscuit names]` — proposal says 17 packaged, names 9 | §7, §9 |
| Packaged stock counts | `[CONFIRM: 0 each — staff set in portal]` | not given |
| University email domain | `[CONFIRM: domain]` — tests use the fixture domain `uni.example` | §4 |
| University list format | `[CONFIRM: CSV  id,email,role,name,department]` | §4 |
| SMTP account | `[CONFIRM: host, port, sender]`; password from env var `CAFE_SMTP_PASSWORD`. Until set, emails go to `data/outbox/` files | §11 |
| Session lifetime / lockout | `[CONFIRM: 8 h]` / `[CONFIRM: 5 failures → 15 min]` | §10 (unspecified) |
| Verification code | `[CONFIRM: 6 digits, 15 min]` | §10 (unspecified) |
| Password length | `[CONFIRM: 8–128 chars]` | §10 (unspecified) |
| Time zone | server's local clock `[CONFIRM: Asia/Dhaka, UTC+6]` | not given |

## Review Focus

1. Cancelled-then-re-reserved churn fills a cooking queue beyond the daily limit → reservation refused with a clear message, never a crash or silent drop (Task 3, Task 5 test `queue_full_rejects_reservation`).
2. Restart while items are Ready mid-window → reminders resume at the correct next-due time, no duplicate "ready" email, items expired during downtime get one expiry email (Task 9 test `restart_rebuilds_reminder_queue_in_due_order`).
3. Two browser tabs reserving the last portion at the same moment → exactly one succeeds (Task 12 E2E `concurrent_last_portion`).
4. A student calling a staff route or another user's reservation ID directly → 403/404, structure unchanged (Task 12 E2E `student_cannot_call_staff_api`, `cannot_cancel_others`).
5. Staff lowering a daily limit below portions already reserved → refused; undo of such a change refused (Task 7 test `limit_below_reserved_refused`).

---

### Task 1: Scaffold, test harness, no-STL guard

**Files:** Create `CMakeLists.txt`, `tests/testing.h`, `tests/test_main.cpp`, `tools/check_no_stl.cmake`, `.gitignore`, `README.md`, `docs/proposal/` (copy of the .docx); `third_party/` per D1.

**Interfaces — Produces:** macros `TEST(name)`, `CHECK(cond)`, `CHECK_EQ(a,b)`; tests self-register into a hand-built linked list; `test_main` prints `PASS name` / `FAIL name (file:line: expr)` and a total, exit code = failures. CTest targets: `unit_tests`, `no_stl_guard`.

- [ ] Step 1: Write `tests/test_harness_selftest.cpp` with one passing and (temporarily) one failing test; run `ctest` → expect 1 failure reported with file:line.
- [ ] Step 2: Remove the failing test; `no_stl_guard` greps the four restricted directories for the forbidden headers listed in Global Constraints and fails on a match; prove it by adding then removing `#include <vector>` in a scratch file.
- [ ] Step 3: `git init`, commit "chore: scaffold, test harness, no-STL guard".

### Task 2: Menu array (`src/ds/menu_array.h/.cpp`)

**Interfaces — Produces:**
`enum class ItemKind { Packaged, Fresh };`
`struct MenuItem { std::string category, name; ItemKind kind; int stock; int dailyLimit; int reservedToday; };`
`const int MAX_MENU_ITEMS = 32;` `struct MenuArray { MenuItem items[MAX_MENU_ITEMS]; int count; };`
`void menuInit(MenuArray&); bool menuAdd(MenuArray&, const MenuItem&); MenuItem* menuAt(MenuArray&, int index); int menuFindByName(const MenuArray&, const std::string&); int menuRemaining(const MenuItem&);`

- [ ] Tests first (`tests/test_menu_array.cpp`): `add_until_full_then_refuses` (32 ok, 33rd false); `at_out_of_range_is_null` (−1, count); `remaining_is_limit_minus_reserved`; `find_by_name_case_sensitive_and_missing_returns_minus1`. Run → fail (link error). Implement. Run → pass. Commit.

### Task 3: Reservation linked list + circular queue (`src/ds/reservation_list.*`, `src/ds/res_queue.*`)

**Interfaces — Produces:**
`enum class ResStatus { Reserved, Ready, Collected, Cancelled, Expired };`
`struct Reservation { int id; std::string user; int itemIndex; int quantity; time_t reservedAt; ResStatus status; time_t readyAt; time_t lastNoticeAt; int day; Reservation* next; };` (`day` = YYYYMMDD)
`struct ReservationList { Reservation* head; Reservation* tail; int size; int nextId; };`
`void rlInit(ReservationList&); Reservation* rlAppend(ReservationList&, const std::string& user, int item, int qty, time_t at, int day);` (O(1), assigns `nextId++`)
`Reservation* rlFindById(const ReservationList&, int id); bool rlDelete(ReservationList&, int id); void rlClear(ReservationList&);`
`struct ResQueue { Reservation** slots; int capacity, front, rear, count; };`
`bool qInit(ResQueue&, int capacity); bool qEnqueue(ResQueue&, Reservation*); Reservation* qDequeue(ResQueue&); Reservation* qPeek(const ResQueue&); bool qIsEmpty(const ResQueue&); bool qIsFull(const ResQueue&); void qFree(ResQueue&);`
`const char* statusName(ResStatus);` (`R-0042` formatting helper: `std::string resCode(int id)`)

- [ ] List tests: `append_sets_head_and_tail`, `ids_increase_from_nextId`, `find_missing_returns_null`, `delete_head_middle_tail_updates_tail_pointer`, `delete_only_node_empties_list`, `clear_frees_all` (size 0, head/tail null).
- [ ] Queue tests: `dequeue_empty_returns_null`, `peek_empty_returns_null`, `fifo_order`, `enqueue_full_returns_false_and_keeps_contents`, `wraparound_after_dequeues` (cap 3: enq 3, deq 2, enq 2 → order preserved, count 3).
- [ ] Run → fail; implement; run → pass; commit.

### Task 4: Action stack (`src/ds/action_stack.*`)

**Interfaces — Produces:**
`enum class ActionType { SetStock, SetDailyLimit, SetSetting, MarkCollected };`
`struct StaffAction { ActionType type; std::string staff; int itemIndex; int reservationId; std::string settingKey; int oldValue; int newValue; time_t at; std::string description; };`
`const int ACTION_STACK_CAPACITY = 50;` `struct ActionStack { StaffAction items[ACTION_STACK_CAPACITY]; int top; };` (`top == -1` empty)
`void stInit(ActionStack&); bool stPush(ActionStack&, const StaffAction&); bool stPop(ActionStack&, StaffAction& out); const StaffAction* stPeek(const ActionStack&); bool stIsEmpty(const ActionStack&); bool stIsFull(const ActionStack&); int stSize(const ActionStack&); const StaffAction* stFromTop(const ActionStack&, int i);`

- [ ] Tests: `pop_empty_returns_false`, `peek_empty_null`, `lifo_order`, `push_full_returns_false_top_unchanged`, `from_top_index_for_history_display`. Implement; pass; commit.

### Task 5: Core rules — reserve and cancel (`src/core/clock.*`, `settings.*`, `cafeteria.*`)

**Interfaces — Produces:**
`struct Clock { virtual time_t now() = 0; virtual ~Clock(); }; struct SystemClock : Clock; struct FakeClock : Clock { time_t t; void set(int y,int m,int d,int hh,int mm,int ss); void advanceMinutes(int); };`
`struct Settings { int cutoffMinute; int cancelDeadlineMinute; int freshnessMinutes; int reminderIntervalMinutes; int perUserLimit; };` (minute-of-day 600 = 10:00) + `bool settingsSet(Settings&, const std::string& key, int value)` with ranges: minute-of-day 0–1439, freshness 30–1440, interval 5–720, per-user 1–10.
`enum class NotifType { Confirmation, Ready, Reminder, Expired, VerificationCode };`
`struct Notification { NotifType type; std::string to, subject, body; int reservationId; Notification* next; }; struct NotificationList { Notification* head; Notification* tail; int count; };` + `nlInit/nlAppend/nlClear`
`struct Cafeteria { MenuArray menu; ReservationList reservations; ResQueue cooking[MAX_MENU_ITEMS]; ResQueue reminders; ActionStack history; Settings settings; Clock* clock; int currentDay; };`
`enum class Result { Ok, BadItem, NotFresh, BadQuantity, CutoffPassed, SoldOut, UserLimit, QueueFull, NotFound, NotOwner, NotCancellable, DeadlinePassed, InvalidValue, NothingToUndo, UndoBlocked };` + `const char* resultMessage(Result)`
`void cafInit(Cafeteria&, Clock*, int cookingCapacity);`
`Result reserve(Cafeteria&, const std::string& user, int item, int qty, NotificationList& out, Reservation** created);`
`Result cancelReservation(Cafeteria&, const std::string& user, int reservationId);`
`int userPortionsToday(const Cafeteria&, const std::string& user, int item);` (non-Cancelled)
`int listUserReservations(const Cafeteria&, const std::string& user, Reservation** outArr, int cap);`

Order of checks in `reserve` (fixes which message the user sees): BadItem → NotFresh → BadQuantity (qty < 1 or > perUserLimit) → CutoffPassed → SoldOut (qty > remaining) → UserLimit → QueueFull. On Ok: append node, `reservedToday += qty`, enqueue cooking queue, append Confirmation ("Reservation R-0042 confirmed: Tehari x 1").

- [ ] Tests (`tests/test_reserve_cancel.cpp`, FakeClock): `reserve_before_cutoff_ok` (09:59:59), `reserve_at_cutoff_refused` (10:00:00), `packaged_item_not_reservable`, `quantity_zero_negative_and_over_limit_refused`, `sold_out_refused_and_partial_refused` (1 left, ask 2), `per_user_limit_across_two_reservations` (1+1 ok, third refused; after cancelling one, a new 1 is ok), `queue_full_rejects_reservation` (capacity 2), `confirmation_notification_text`, `cancel_before_deadline_returns_portion`, `cancel_after_deadline_keeps_portion`, `cancel_twice_refused`, `cancel_others_reservation_is_NotOwner`, `cancel_ready_reservation_refused`, `settings_change_moves_cutoff` (cutoff 11:00 → 10:30 reserve ok).
- [ ] Run → fail; implement; run → pass; commit.

### Task 6: Core — ready, collected, reminders, expiry, day rollover

**Interfaces — Produces:**
`int markReady(Cafeteria&, const std::string& staff, int item, int portions /*0 = all waiting*/, NotificationList& out);` → number of reservations made Ready. Dequeue FIFO; skip non-Reserved; stop when the next reservation's quantity would exceed the remaining `portions` (it stays at the front). Each: status Ready, readyAt = lastNoticeAt = now, Ready email ("Your Tehari (R-0042) is ready for collection. It stays fresh for about 7 hours."), enqueue in reminders.
`Result markCollected(Cafeteria&, const std::string& staff, int reservationId);` (Ready → Collected; pushes MarkCollected per D4)
`void timerTick(Cafeteria&, NotificationList& out);` — (a) day rollover per D9; (b) loop: peek reminders front; dequeue and drop if not Ready; if `now >= readyAt + freshness` → Expired + expiry email, drop; else if `now >= lastNoticeAt + interval` → Reminder email ("About N hours of freshness left", N rounded down, minutes when < 1 h), lastNoticeAt = now, re-enqueue at rear; else stop (front not due ⇒ none due).

- [ ] Tests (`tests/test_ready_reminders.cpp`): `ready_emails_in_fifo_order`, `ready_skips_cancelled`, `ready_batch_stops_at_portion_count` (portions 3, queue qty 2,2 → only first Ready, second stays at front), `ready_on_empty_queue_returns_0`, `no_reminder_before_interval` (59 min), `reminder_at_interval_and_requeued`, `seven_hour_window_gives_6_reminders_then_expiry` (tick every minute for 7 h: exactly 6 Reminder + 1 Expired), `collected_stops_reminders`, `collect_reserved_refused`, `rollover_expires_uncooked_and_resets_counts`.
- [ ] Implement; pass; commit.

### Task 7: Core — staff inventory, settings, undo, staff list

**Interfaces — Produces:**
`Result setStock(Cafeteria&, const std::string& staff, int item, int value, bool& undoRecorded);` (packaged only, 0–9999)
`Result setDailyLimit(Cafeteria&, const std::string& staff, int item, int value, bool& undoRecorded);` (fresh only, `reservedToday ≤ value ≤ cookingCapacity`)
`Result changeSetting(Cafeteria&, const std::string& staff, const std::string& key, int value, bool& undoRecorded);`
`Result undoLast(Cafeteria&, const std::string& staff, StaffAction& undone);` — pop; restore `oldValue`; MarkCollected → back to Ready, lastNoticeAt = now, re-enqueue in reminders; if restoring would violate a rule (limit below reserved, reservation no longer Collected) → `UndoBlocked` and the action is pushed back.
`int staffReservationsSorted(const Cafeteria&, Reservation** outArr, int cap);` — today's reservations, insertion sort by (itemIndex, reservedAt) — O(k²) per §13.
`Reservation* staffSearchById(const Cafeteria&, int id);`

- [ ] Tests (`tests/test_staff_undo.cpp`): `set_stock_then_undo_restores`, `undo_empty_is_NothingToUndo`, `undo_is_lifo_across_three_actions`, `limit_below_reserved_refused`, `undo_blocked_when_limit_now_below_reserved` (stack top unchanged), `stack_full_action_applies_but_not_recorded` (51st action: applied, `undoRecorded == false`), `undo_collected_restores_ready_and_reminders`, `mark_ready_not_on_stack`, `sorted_by_item_then_time`, `search_by_id_hit_and_miss`.
- [ ] Implement; pass; commit.

### Task 8: Console demo (stage 2 milestone, §14)

**Files:** `src/console/console_demo.cpp` → target `cafeteria_console`.
Menu-driven program over `Cafeteria` with a FakeClock: show menu, reserve, cancel, list mine, staff mark ready/collected/set limit/undo/show stack, advance clock, tick, show queue contents front→rear. Notifications print to the console.

- [ ] Write `tests/console_script.txt` (scripted stdin covering reserve → cancel → ready → reminders → expiry → undo) and a CTest that runs the binary with it and checks the transcript contains the expected lines (`R-0001 confirmed`, `skipped R-0002 (Cancelled)`, `Expired`). Run → fail; implement; pass; commit.

### Task 9: Storage and restart recovery (`src/storage/storage.*`)

**Interfaces — Produces:** `bool loadCafeteria(Cafeteria&, const std::string& dir, std::string& err); bool saveCafeteria(const Cafeteria&, const std::string& dir, std::string& err); bool loadUsers(UserList&, ...); bool saveUsers(...)` (file formats in ADR-001). Atomic write: `<file>.tmp` then `MoveFileExA(..., MOVEFILE_REPLACE_EXISTING)`. Rebuild: cooking queues from Reserved in list order; reminders from Ready sorted by `lastNoticeAt` (insertion sort on a `new[]` pointer array); stack empty; `nextId = max id + 1`.

- [ ] Tests (`tests/test_storage.cpp`, temp dir): `round_trip_menu_settings_reservations`, `restart_rebuilds_cooking_queue_skipping_cancelled`, `restart_rebuilds_reminder_queue_in_due_order`, `restart_stack_is_empty`, `ids_continue_after_restart`, `ready_item_expired_during_downtime_expires_on_first_tick` (exactly one expiry email), `corrupt_line_reports_line_number_and_refuses_start`, `missing_files_start_from_seed`.
- [ ] Implement; pass; commit.

### Task 10: Auth and validation (`src/auth/`)

**Interfaces — Produces:**
`enum class Role { Student, Teacher, Staff };`
`struct User { std::string login, universityId, name, department, passwordHash, codeHash; Role role; bool verified; int failedLogins; time_t lockedUntil, codeExpires; User* next; }; struct UserList { User* head; User* tail; int count; };`
`struct UniEntry { std::string id, email, name, department; Role role; UniEntry* next; };` loaded from `config/university_list.csv`.
`struct Session { std::string token, login; Role role; time_t expiresAt; Session* next; }; struct SessionList { Session* head; int count; };`
`bool cryptoInit(); std::string hashPassword(const std::string&); bool verifyPassword(const std::string& hash, const std::string& pw); std::string randomToken(int bytes);` (libsodium `crypto_pwhash_str`/`randombytes_buf`, or CNG per D1)
`enum class AuthResult { Ok, NotUniversityEmail, IdNotListed, EmailIdMismatch, AlreadyRegistered, WeakPassword, BadCode, CodeExpired, BadCredentials, Locked, NotVerified };`
`AuthResult registerStart(AuthState&, email, id, password, NotificationList& out);` (role copied from the list entry) · `AuthResult registerVerify(AuthState&, email, code);` · `AuthResult login(AuthState&, login, password, bool staffPortal, std::string& tokenOut);` (staff can log in only via the staff portal and students/teachers only via the public one) · `Session* sessionGet(AuthState&, token);` (expired → removed, null) · `void logout(AuthState&, token);`
`validation.h`: `bool validEmail(s)`, `bool validUniversityId(s)` (`[CONFIRM format]`; digits, 6–20), `bool validName(s)` (1–60, no `|` or control chars), `bool parseIntStrict(const std::string&, int& out)` (rejects "", "+", "1e3", "12abc", overflow).
Staff admin CLI: `src/console/cafeteria_admin.cpp add-staff <login> <name>` per D10.

- [ ] Tests (`tests/test_auth.cpp`, fixture list on `uni.example`): `non_university_email_rejected`, `unlisted_id_rejected`, `id_email_mismatch_rejected`, `role_comes_from_list`, `verification_code_correct_wrong_expired`, `login_before_verify_refused`, `hash_is_salted` (same password → different hashes, both verify), `lockout_after_5_failures_then_unlock_after_15min`, `staff_cannot_use_public_login_and_vice_versa`, `session_expires`, `logout_removes_session`, `tokens_unique_and_32_bytes`, validation: `negative_and_non_numeric_quantity`, `long_text_over_limit`, `pipe_and_newline_rejected`.
- [ ] Implement; pass; commit.

### Task 11: Email (`src/email/`)

**Interfaces — Produces:** `struct EmailSender { virtual bool send(const Notification&) = 0; virtual ~EmailSender(); }; struct OutboxSender : EmailSender` (writes `data/outbox/NNNNNN_<type>.txt`, also appends to `data/outbox/index.log`) ; `struct SmtpCurlSender : EmailSender` (builds an RFC 5322 message file, runs `curl.exe --ssl-reqd --url smtps://HOST:PORT --mail-from ... --mail-rcpt ... --upload-file ... --user SENDER:%CAFE_SMTP_PASSWORD%` with `CreateProcessA`, argument list built without a shell; recipient validated by `validEmail` first). `EmailSender* makeSender(const SmtpConfig&)` → Outbox when config holds `[CONFIRM]`.

- [ ] Tests: `outbox_writes_one_file_per_notification_with_to_subject_body`, `smtp_command_line_has_no_shell_metacharacters_from_input` (pure function `buildCurlArgs`), `header_injection_newline_in_subject_rejected`. Manual SMTP check only after you give an account. Implement; pass; commit.

### Task 12: HTTP server, roles, mutex, timer (`src/server/`)

**Files:** `main.cpp`, `routes.cpp`, `json.cpp` (hand-escaped JSON output), `app_state.h` (`struct AppState { Cafeteria caf; AuthState auth; std::mutex mu; EmailSender* email; std::string dataDir; }`).
Routes (form-encoded in, JSON out; every `/api/staff/*` checks `role == Staff`, every other `/api/*` except register/verify/login checks Student/Teacher):
`POST /api/register` · `POST /api/verify` · `POST /api/login` · `POST /api/staff/login` · `POST /api/logout` · `GET /api/me` · `GET /api/menu` · `POST /api/reservations` · `GET /api/reservations/mine` · `POST /api/reservations/cancel` · `GET /api/staff/reservations?q=` · `POST /api/staff/stock` · `POST /api/staff/limit` · `POST /api/staff/ready` · `POST /api/staff/collected` · `POST /api/staff/undo` · `GET /api/staff/history` · `GET|POST /api/staff/settings` · `POST /api/dev/advance` (only with `--fake-clock`, only from 127.0.0.1).
Handler shape: validate → `{ lock_guard; session; role; core call; save; }` → send notifications → respond. Cookie: `session=<token>; HttpOnly; SameSite=Strict; Path=/` (+ `Secure` when `--behind-https`). Request body cap 4 KB. Unknown route → 404 JSON. Flags: `--port`, `--data`, `--fake-clock`, `--tick-seconds`.

- [ ] E2E test program `tests/e2e/e2e_section12.cpp` (httplib client; starts the server on a free port with a temp data dir, `--fake-clock`, outbox email) — each §12 item is one named check, printing PASS/FAIL. Written now, run → fail; implement server; run → pass. Includes `student_cannot_call_staff_api`, `cannot_cancel_others`, `concurrent_last_portion` (20 threads, 1 portion left → exactly 1 Ok). Commit.

### Task 13: Web pages (`web/`)

Invoke **frontend-design** first, then **design:accessibility-review** on the result.
Pages: `index.html` (login), `register.html` (register + verify code), `home.html` (today's food: fresh with remaining/cutoff + reserve form; packaged view-only; profile card shows student ID for students, department for teachers), `reservations.html` (mine + cancel), `staff/login.html`, `staff/portal.html` (inventory, limits, today's reservations sorted + search by R-id, mark ready with portion count, mark collected, undo + history list, settings). Shared `app.css`, `app.js` (fetch wrapper; all server text inserted with `textContent`, never `innerHTML`).

- [ ] Check in the built-in browser at 375 px and desktop width, keyboard-only pass, contrast check; fix; commit.

### Task 14: Full test run, code review, verification

- [ ] Invoke **engineering:testing-strategy**; map each §12 bullet to the unit/E2E check that covers it in `docs/testing/test-report.md`.
- [ ] `ctest --test-dir build --output-on-failure` and `build/e2e_section12.exe`; paste real output into the report.
- [ ] Restart check: run server, reserve + mark ready, kill the process, restart, verify reservations, queues and reminder timing via E2E `restart_recovery`.
- [ ] Invoke **engineering:code-review** on `src/` (incl. no-STL guard result); fix findings with **superpowers:systematic-debugging** where a test fails.
- [ ] Invoke **superpowers:verification-before-completion**; commit.

### Task 15: "Suggested additions" list (no code)

Write `docs/suggested-additions.md`, every item marked `[optional]`, then stop and wait for your decision.
