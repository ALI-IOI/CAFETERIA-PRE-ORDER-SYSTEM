# Test report — proposal section 12

**Date:** 2026-10-24 (re-run after university staff, the hidden staff terminal and the eggshell theme) · **Build:** MinGW-w64 g++ 13.1, `-Wall -Wextra`, 0 warnings
**Run:** `ctest --test-dir build --output-on-failure`

| Suite | What it is | Result |
|---|---|---|
| `unit_tests` | 221 unit tests: data structures (incl. linked-list merge sort), rules, storage, auth, email, lock file, waitlist, email retry, menu items, tray, pictures, order history, password change, university staff, staff computer list (fake clock) | **221 / 221 pass** |
| `no_stl_guard` | Fails the build if `vector/list/queue/stack/deque/map/set/array` appear in `src/ds`, `src/core`, `src/auth`, `src/storage`, `src/console`, `src/email` | **pass** (proven to fail on an injected `#include <vector>`) |
| `console_demo` | Scripted run of the stage-2 console program, checks the transcript | **pass** |
| `admin_tool` | `add-staff` and `add-person`: hashed, duplicate/short/mismatched password refused, a student ID refused for `add-person`, refused while a live server holds the lock, allowed with a stale lock | **pass** |
| `thread_exit_stress` | 640 short-lived client threads against an in-process HTTP server (guards the MinGW thread-exit crash fix below) | **pass** |
| `e2e_section12` | Starts the real server (fake clock, temp data, email outbox), drives it over HTTP, crashes and restarts it | **113 / 113 checks pass** |

Raw output: [ctest_summary.txt](ctest_summary.txt), [unit_tests_output.txt](unit_tests_output.txt),
[e2e_section12_output.txt](e2e_section12_output.txt).

## Section 12, item by item

| # | Proposal test | Covered by | Result |
|---|---|---|---|
| 1 | Register with university email + listed ID, verification code works; non-university email and unlisted ID rejected | E2E `12.1 *` (6 checks); unit `non_university_email_rejected`, `unlisted_id_rejected`, `id_email_mismatch_rejected`, `verification_code_correct_wrong_expired`, `verification_code_locked_after_5_wrong_attempts`, `login_before_verify_refused`, `role_comes_from_list` | Pass |
| 2 | Student, teacher, staff each see the correct interface; staff portal refused to a student | E2E `12.2 *` (student ID vs department profile, staff-only login, page redirect, 403 on staff API); unit `staff_cannot_use_public_login_and_vice_versa`; browser check of all three views | Pass |
| 3 | Valid reservation appears in My Reservations; confirmation email | E2E `12.3 *`; unit `reserve_before_cutoff_ok`, `confirmation_notification_text` | Pass |
| 4 | After cutoff, sold-out item, over the per-user limit — each rejected | E2E `12.4 reservation at 09:59 accepted` / `at 10:00 (cutoff) rejected`, `sold-out`, `per-user limit`, `packaged`; unit `reserve_at_cutoff_refused`, `sold_out_refused_and_partial_refused`, `per_user_limit_across_two_reservations` | Pass |
| 5 | Cancel before and after deadline; portion returns only before | E2E `12.5 *`; unit `cancel_before_deadline_returns_portion`, `cancel_after_deadline_keeps_portion`, `cancel_twice_refused` | Pass |
| 6 | Ready emails in FIFO order, cancelled skipped | E2E `12.6 ready emails in FIFO order, cancelled skipped` (order ayon → teacher → s12 → s3, s11 cancelled gets nothing); unit `ready_emails_in_fifo_order`, `ready_skips_cancelled`, `ready_batch_stops_at_portion_count` | Pass |
| 7 | Hourly reminders, stop when collected, expiry email at end of window | E2E: 6 reminders in the 7 h window, none after collection, 1 expiry; unit `seven_hour_window_gives_6_reminders_then_expiry`, `collected_stops_reminders`, `reminders_interleave_in_due_order_for_two_items` | Pass |
| 8 | Queue empty/full, stack empty/full, undo a staff action | Unit `queue_dequeue_empty_returns_null`, `queue_peek_empty_returns_null`, `queue_enqueue_full_returns_false_and_keeps_contents`, `queue_wraparound_after_dequeues`, `queue_full_rejects_reservation`, `stack_pop_empty_returns_false`, `stack_push_full_returns_false_top_unchanged`, `stack_full_action_applies_but_not_recorded`, `undo_is_lifo_across_three_actions`, `undo_blocked_when_limit_now_below_reserved`; console demo "Nothing to undo"; E2E `12.8 undo restores Lays stock`, `stack empty after restart` | Pass |
| 9 | Invalid input: negative quantities, very long text, special characters, wrong passwords | E2E `12.9 *` (negative / non-numeric quantity, unknown item, 5000-char email, `<script>` login, SQL-style search text, `25:99` time, 5 wrong passwords → locked); unit `negative_and_non_numeric_quantity`, `long_text_over_limit`, `pipe_and_newline_rejected`, `lockout_after_5_failures_then_unlock_after_15min`, `header_injection_newline_in_subject_rejected` | Pass |
| 10 | Restart restores menu, users, reservations | E2E: process killed at 12:00 mid-window and restarted — users, reservations, limits restored, stack empty, reminders resume with no duplicate "ready" email, expiry at 17:00; unit `restart_rebuilds_cooking_queue_skipping_cancelled`, `restart_rebuilds_reminder_queue_in_due_order`, `ready_item_expired_during_downtime_expires_on_first_tick`, `restart_next_day_runs_rollover`, `ids_continue_after_archive`, `corrupt_line_reports_line_number_and_refuses_start` | Pass |

## Review-focus cases (beyond section 12)

| Case | Test | Result |
|---|---|---|
| 10 people reserve the last portion at the same moment | E2E `concurrent_last_portion` — exactly 1 succeeds, 9 "sold out" (mutex works) | Pass |
| Student calls a staff API directly | E2E `student_cannot_call_staff_api` → 403 | Pass |
| Cancel someone else's reservation | E2E `cannot_cancel_others` → 404, unit `cancel_others_reservation_is_NotOwner` | Pass |
| Cross-site POST (CSRF) | E2E `POST without X-Cafe header refused` → 403 | Pass |
| Lower a limit below portions already reserved | Unit `limit_below_reserved_refused`, `undo_blocked_when_limit_now_below_reserved` | Pass |

## Fixes from the final code review (each test seen failing first)

| Finding | Regression test | Before fix | After |
|---|---|---|---|
| A reservation made after midnight but before the first timer tick was refused ("sold out" from yesterday) or wiped from the cooking queue by that tick | unit `reserve_after_midnight_before_tick_is_kept` | FAIL | Pass |
| Someone knowing a student's email + ID could re-register during the pending window and have the student's code verify the stranger's password | unit `pending_registration_cannot_be_taken_over`; E2E `verify with a different password refused` | compile fail (no password check) | Pass |
| Changing freshness/interval moved the deadline of food already Ready (against decision D7) | unit `freshness_change_after_ready_keeps_original_deadline`, `ready_deadline_and_interval_survive_restart`, `old_nine_field_reservation_lines_still_load` | compile fail (no stored deadline) | Pass |
| A second server on the same data folder overwrote the live lock; a crash's stale lock blocked the admin tool | unit `test_lock_file.cpp` (4); admin-tool stale-lock case; E2E `second server on the same data folder refuses` | E2E FAIL against the pre-fix server (exit 259, lock overwritten) | Pass |

## Additions #1 (waitlist) and #2 (email retry)

| Area | Tests | Result |
|---|---|---|
| Waitlist queue (circular, hand-built) | unit `waitq_*` (5): empty, FIFO, full, wraparound, clear | Pass |
| Waitlist rules | unit `test_waitlist.cpp` (19): join only when sold out, once, within limit, before cutoff; strict FIFO promotion on cancel / limit raise / undo; skip people at their limit; close at cutoff with one email; rollover; compaction; direct reserve leaves the list; promotion after a full cooking queue empties; cancelling never re-books the canceller | Pass |
| Waitlist on restart | unit `waitlist_order_survives_restart`, `left_entries_not_saved`, `old_day_waitlist_not_loaded`, `corrupt_waitlist_line_refuses_start` | Pass |
| Email retry queue (linked, hand-built) | unit `test_mailer.cpp` (11): failure queued; retry after 1 min; 1/5/15 then give up and log; order kept; not-due items wait; survives restart; escaping; new mail waits behind the same person's older mail; other people not held up; damaged file refused | Pass |
| End to end | E2E: join at position 1, join twice refused, cannot join while portions remain, place in line shown, cancellation promotes with email, portion used once, list closes at cutoff with email; outbox blocked → confirmation waits → delivered 1 minute after recovery | Pass |
| Console demo | scripted waitlist scenario (strict FIFO, cutoff email) | Pass |

**Intermittent E2E failures seen during development (now explained and fixed):** a few runs failed (a request with no response, an abnormal exit after the last check) or hung. Cause, confirmed with a gdb backtrace: heap corruption (`0xC0000374`) when a server worker thread exits. MinGW-w64 runs the destructors of `thread_local` objects after the thread's memory is already freed; cpp-httplib keeps several read-only regexes and sets as `thread_local`. Fix: those five objects are `static const` instead (`third_party/httplib/PATCHES.md`). Evidence: the new `thread_exit_stress` test crashed 5 of 5 times before the patch and passed 10 of 10 after; `tools/e2e_gdb_hunt.sh` ran the E2E suite 25 times under gdb with no crash.

## Redesign (menu items, tray, pictures, landing page)

| Area | Tests | Result |
|---|---|---|
| Staff menu items | unit `test_menu_items.cpp` (10): add at next index, refused when the 32-item array is full, duplicate and bad names refused, price change + undo, "not available today" refuses reserve and waitlist, existing reservations kept, waitlist paused while unavailable, undo of availability, picture change not undoable | Pass |
| Tray (several dishes at once) | unit `test_batch.cpp` (9): all or nothing, one confirmation email with every code and the total | Pass |
| Pictures | unit `test_image_store.cpp` (7): JPEG/PNG/WebP recognised by their first bytes, anything else refused, 2 MB limit, server-chosen file names only | Pass |
| End to end | E2E `redesign:` checks (12): public info without login, staff add item, student sees it with price, upload then served, non-image refused, students cannot upload, other routes keep the 4 KB limit, names shown as text, unavailable item hidden and refused, tray with one bad line reserves nothing, tray reserves all with one email | Pass |
| Look and feel | `python tools/contrast_check.py`: 24 colour pairs, light and dark, 0 failed. Pages checked in a browser at desktop and 375 px phone width (no sideways scrolling) | Pass |

## Order history and My account

| Area | Tests | Result |
|---|---|---|
| Merge sort on the linked list | unit `test_reservation_sort.cpp` (5): empty and single, newest day then later time first, ties broken by higher id, already sorted and reversed input, 1,000-node list keeps every node and the tail | Pass |
| History file | unit `test_history.cpp` (4): only that person's orders, newest first; missing file is empty; damaged line reported; after midnight it holds yesterday's orders | Pass |
| Password change | unit `change_password_*` (3): other sessions signed out, wrong current password counts toward lockout, weak or unchanged new password refused | Pass |
| End to end | E2E `history:` / `account:` checks (8): yesterday's orders listed after midnight, newest first, only your own, needs a login; wrong current password refused; changed password keeps this session; old password stops working; staff refused (they use the admin tool) | Pass |
| Pages | Browser check: "Order history" table under My reservations; My account shows the university details and changes the password (mismatch, wrong current, success messages); logged-out visitors are redirected | Pass |

## Fixes from the final review of the redesign (each test seen failing first)

| Finding | Test | Before → after |
|---|---|---|
| A category starting with `#` made the item vanish on restart (the line read as a comment) and shifted later items | unit `category_starting_with_hash_refused` | FAIL → PASS |
| Multipart bodies skipped the 4 KB limit (they go to `req.form`, not `req.body`); now refused from the headers, before the body is read | E2E `redesign: multipart_body_keeps_4kb_limit` | 401 → 413 |
| One damaged line in `history.txt` broke everyone's history page; now skipped and logged | unit `history_bad_line_skipped_and_reported` | FAIL → PASS |
| The cutoff passing between checking and booking a tray could crash the server with half the tray booked | unit `batch_cutoff_passing_mid_booking_is_all_or_nothing` | crash → PASS |

## University staff, hidden staff terminal, eggshell theme

| Area | Tests | Result |
|---|---|---|
| Employee role and university list | unit `test_employee.cpp`: role round-trip; blank email allowed only on `employee` rows (refused on a student row with the line number); duplicate email across rows refused; `users.txt` keeps the role | Pass |
| Sign-up rules | unit: employee with a university email signs up, verifies and logs in; a blank-email row is refused with "Ask the cafeteria office to create your account."; an admin-added person cannot self-register afterwards (by ID or with the personal email) and their password is untouched | Pass |
| `addPerson` / `add-person` | unit: active account with the list's name and department; refusals (student ID, row that has an email, unknown ID, email of another account or row, same ID twice, weak password, invalid email); mixed-case email stored lower-case, login in any case. `admin_tool`: success, refusals, live lock | Pass |
| Same rules as teachers | unit `employee_same_limit_as_teacher`: per-person limit applies alike | Pass |
| End to end | E2E `employee:` (3): university-email sign-up and reservation; blank-email row refused with the office message; admin-added account logs in, reserves and gets the confirmation email. `pages: copy names university staff` | Pass |
| Staff computer list (fixed array) | unit `test_staff_computers.cpp` (8): missing file = empty; comments and blanks; bad address refused with its line; more than 16 refused; empty list allows all; exact match only; `::ffff:a.b.c.d` matches `a.b.c.d`; `isStaffPath` ignores case and matches any `staff` segment | Pass |
| Hidden staff terminal | E2E (4): no public page links to `/staff/`; with only `10.20.30.40` listed, staff pages and APIs (GET and POST) answer exactly like a missing page; `/STAFF/login.html`, `/Staff/portal.html`, `/staff`, `/api/staff`, `/x/../staff/login.html` too (before the fix Windows served the uppercase and `..` spellings: 200); with an empty list the start-up notice is printed and the page opens | Pass |
| Eggshell theme | E2E `theme:` (2): `/api/public/info` says whether orders are open (true before the cutoff, false after); the stylesheet has no dark-mode rules and sets `color-scheme: light`. `tools/contrast_check.py`: 19/19 pairs. Browser: 8 pages at 375 px with dark mode emulated — eggshell background, 0 dark elements, no sideways scroll | Pass |

## Gaps (not covered automatically)

- **Real SMTP delivery** — no account yet (`[CONFIRM]`). Only message building and the curl command line are unit-tested. Test once an account is set: send one confirmation to a team member.
- **Wall-clock timer thread** — tests drive time through the fake clock. Manual check (2026-10-23): server on the real clock with `--tick-seconds 2` re-saved its data files on each tick (12:55:50 → 12:55:56) while answering requests. A real one-hour reminder was not waited for.
- **HTTPS** — not run locally (proposal §14 allows simplifying it); see README for deploying behind a reverse proxy.
- **Screen-reader pass** — see `docs/accessibility-review.md`.

## Fixes from the final review of university staff and the hidden terminal (each test seen failing first)

| Finding | Test | Before → after |
|---|---|---|
| Windows also opens the staff folder as `staff.`, `staff%2e` and `staff::$INDEX_ALLOCATION`, so those spellings served the staff pages from any computer | unit `is_staff_path_windows_spellings`; E2E `staff_hidden_case_and_path_tricks` (spellings added) | 200 → same as a missing page |
| The staff pages' scripts were public at `/js/staff-*.js` and named every staff API | E2E `staff hidden: staff scripts are served from /staff/` and the tricks list | 200 → hidden (moved to `web/staff/`) |
| Staff without a university email who typed a personal one got "use your university email" instead of the office message | unit `blank_email_row_personal_email_gets_office_message`, `register_after_add_person_refused` | NotUniversityEmail → NoUniversityEmail |
| Page copy left out university staff ("Student or teacher ID", "University email", "Student & teacher login") | E2E `pages: no copy that leaves out university staff` | FAIL → PASS |
