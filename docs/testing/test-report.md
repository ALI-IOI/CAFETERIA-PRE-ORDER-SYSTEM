# Test report — proposal section 12

**Date:** 2026-10-23 · **Build:** MinGW-w64 g++ 13.1, `-Wall -Wextra`, 0 warnings
**Run:** `ctest --test-dir build --output-on-failure`

| Suite | What it is | Result |
|---|---|---|
| `unit_tests` | 107 unit tests: data structures, rules, storage, auth, email (fake clock) | **107 / 107 pass** |
| `no_stl_guard` | Fails the build if `vector/list/queue/stack/deque/map/set/array` appear in `src/ds`, `src/core`, `src/auth`, `src/storage`, `src/console` | **pass** (proven to fail on an injected `#include <vector>`) |
| `console_demo` | Scripted run of the stage-2 console program, checks the transcript | **pass** |
| `admin_tool` | Staff account creation: hashed, duplicate/short/mismatched password refused, refused while server runs | **pass** |
| `e2e_section12` | Starts the real server (fake clock, temp data, email outbox), drives it over HTTP, crashes and restarts it | **65 / 65 checks pass** |

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

## Gaps (not covered automatically)

- **Real SMTP delivery** — no account yet (`[CONFIRM]`). Only message building and the curl command line are unit-tested. Test once an account is set: send one confirmation to a team member.
- **Wall-clock timer thread** — tests drive time through the fake clock; the 60-second background thread is exercised only by a manual run.
- **HTTPS** — not run locally (proposal §14 allows simplifying it); see README for deploying behind a reverse proxy.
- **Screen-reader pass** — see `docs/accessibility-review.md`.
