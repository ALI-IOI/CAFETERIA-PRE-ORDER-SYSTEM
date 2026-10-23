# University Cafeteria Food Pre-Order System (BAUSTK, DSA I)

A C++ web system in which students and teachers reserve freshly cooked food before a
cutoff, staff mark food ready, and everyone gets emails. **All reservation logic runs on
four hand-built data structures** — no STL containers for them (checked by a build test).

| Structure | Where | Used for |
|---|---|---|
| Array | `src/ds/menu_array.*` | Menu, packaged stock, fresh portions (O(1) by index) |
| Linked list | `src/ds/reservation_list.*`, `src/auth/users.*` | Reservations (tail insert O(1), search/cancel O(n)); user accounts; university list; sessions |
| Circular queue | `src/ds/res_queue.*` | One cooking queue per fresh item (FIFO "ready" emails, cancelled entries skipped); the reminder queue (front = next reminder due) |
| Array stack | `src/ds/action_stack.*` | Staff action history; "undo the latest action" (LIFO) |
| Circular queue (addition) | `src/ds/wait_queue.*` | Waitlist per sold-out fresh item: a freed portion goes to the first person waiting |
| Linked queue (addition) | `src/email/mailer.*` | Emails that failed to send, retried after 1, 5 and 15 minutes |

Rules live in `src/core/` (`cafeteria.cpp`, `staff.cpp`), storage in `src/storage/`,
security in `src/auth/`, email in `src/email/`, the HTTP server in `src/server/`, pages in `web/`.
Design: `docs/architecture/ADR-001-server-architecture.md`. Plan: `docs/superpowers/plans/`.
Test results: `docs/testing/test-report.md`.

## Build and test

Needs a C++17 compiler (tested: MinGW-w64 g++ 13.1 on Windows), CMake and Ninja.
cpp-httplib and libsodium are included in `third_party/`.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## Programs

| Program | Purpose |
|---|---|
| `build/cafeteria_console` | Stage-2 console demo: drive the structures directly, see queues and the stack, move a fake clock (`help` lists commands) |
| `build/cafeteria_server` | The web server (default http://127.0.0.1:8080) |
| `build/cafeteria_admin` | Creates staff accounts (stop the server first) |
| `build/e2e_section12` | End-to-end test of the proposal's section 12 plan (run by `ctest`) |

## Try it locally

See `demo/README.md`: it runs the server with a fake university list (`uni.example`), a demo
staff account and a fake clock you can move forward to show reminders and expiry.

## Running for real

1. Fill every `[CONFIRM]` placeholder — the server prints the remaining ones at start-up:
   - `config/app.cfg` — university email domain, session/lockout times
   - `config/university_list.csv` — the official student/teacher list (format in `config/university_list.sample.csv`)
   - `config/smtp.cfg` — the sender account; set the password in the `CAFE_SMTP_PASSWORD` environment variable
   - `seed/menu.txt`, `seed/settings.txt` — menu, daily limits, cutoff, freshness, reminder interval (used on first start only; staff change them in the portal later)
2. Create staff: `build/cafeteria_admin add-staff <login> "<full name>"`
3. Start: `build/cafeteria_server` (options: `--port`, `--host`, `--data`, `--config`, `--behind-https`)

Data lives in `data/` (text files, written atomically). Until SMTP is configured, emails are
saved as files in `data/outbox/`.

**HTTPS:** for deployment, put the server behind a reverse proxy that terminates HTTPS (e.g.
Caddy or nginx) and start it with `--behind-https` so cookies are marked `Secure`.

## Additions beyond the proposal
- **Waitlist** — when a fresh item is sold out, students and teachers can join its waitlist. A cancellation, or staff raising the daily limit, gives the freed portions to the people waiting, first come first served, with a confirmation email. At the cutoff, anyone still waiting gets one "no portion became free" email.
- **Email retry** — an email that fails to send is retried after 1, 5 and 15 minutes, then logged in `data/email_failed.log`. Waiting emails are kept in `data/pending_emails.txt` across restarts. The staff portal shows how many are waiting.

## Known limits (documented, proposal §10)
- Sessions are kept in memory; a restart logs everyone out.
- One coarse mutex serialises requests — plenty for one cafeteria.
- Real SMTP delivery is untested until an account is provided.
