# University Cafeteria Food Pre-Order System (BAUSTK, DSA I)

A C++ web system in which everyone at the university (students, teachers and university
staff, from the VC's office to the bus drivers) reserves freshly cooked food before a
cutoff, staff mark food ready, and everyone gets emails. **All reservation logic runs on
four hand-built data structures** — no STL containers for them (checked by a build test).

**Live demo:** https://ali-ioi.github.io/CAFETERIA-PRE-ORDER-SYSTEM/ — the real pages from `web/`
running on an in-browser stand-in for the server (`pages-demo/demo-api.js`): fake data kept in
your browser, no emails, clock fixed at 08:30. Try `ayon@uni.example` / `demo-student-pass`,
`guard1@uni.example` / `demo-guard-pass`, or staff `demo.staff` / `demo-staff-pass` at
`staff/login.html`. The C++ server itself (data structures, persistence, emails, security) runs
locally: see `demo/README.md`. The student IDs in the sample lists and tests are the team's real
university IDs; every email uses the reserved test domain `uni.example`.

| Structure | Where | Used for |
|---|---|---|
| Array | `src/ds/menu_array.*` | Menu, packaged stock, fresh portions (O(1) by index) |
| Linked list | `src/ds/reservation_list.*`, `src/auth/users.*` | Reservations (tail insert O(1), search/cancel O(n)); user accounts; university list; sessions; order history (merge sort, newest first, O(n log n)) |
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
| `build/cafeteria_admin` | Creates cafeteria staff accounts (`add-staff`) and accounts for university staff with no university email (`add-person`); stop the server first |
| `build/e2e_section12` | End-to-end test of the proposal's section 12 plan (run by `ctest`) |
| `build/contrast_check` | WCAG contrast of every colour pair the pages use (run by `ctest`) |
| `build/check_hex_colors` | Fails if any colour in `web/` or `pages-demo/` is not written in hex (run by `ctest`) |
| `build/build_demo` | Builds the GitHub Pages demo from `web/` (run by `ctest` and by the Pages workflow) |

Everything is C/C++ (plus CMake for the build and its test scripts); the only other languages are
the web pages themselves (HTML, CSS, JavaScript), which the browser needs. To repeat a test until it
fails, use CTest itself: `ctest --test-dir build -R e2e --repeat until-fail:20`.

## Try it locally

See `demo/README.md`: it runs the server with a fake university list (`uni.example`), a demo
staff account and a fake clock you can move forward to show reminders and expiry.

## Running for real

1. Fill every `[CONFIRM]` placeholder — the server prints the remaining ones at start-up:
   - `config/app.cfg` — university email domain, session/lockout times
   - `config/university_list.csv` — the official list of students, teachers and university staff (`student`, `teacher` or `employee`; format in `config/university_list.sample.csv`). Only `employee` rows may leave the email blank
   - `config/staff_computers.txt` — the cafeteria counter computers' IP addresses (one per line). Once it lists any, every other computer gets "not found" for the staff terminal
   - `config/smtp.cfg` — the sender account; set the password in the `CAFE_SMTP_PASSWORD` environment variable
   - `seed/menu.txt`, `seed/settings.txt` — menu, daily limits, cutoff, freshness, reminder interval (used on first start only; staff change them in the portal later)
2. Create cafeteria staff: `build/cafeteria_admin add-staff <login> "<full name>"`.
   University staff with no university email (bus drivers, guards, cleaners): `build/cafeteria_admin add-person <university ID> <personal email>`; they change the starting password on My account
3. Start: `build/cafeteria_server` (options: `--port`, `--host`, `--data`, `--config`, `--behind-https`)

Data lives in `data/` (text files, written atomically). Until SMTP is configured, emails are
saved as files in `data/outbox/`.

**HTTPS:** for deployment, put the server behind a reverse proxy that terminates HTTPS (e.g.
Caddy or nginx) and start it with `--behind-https` so cookies are marked `Secure`. Behind a proxy
the server sees the proxy's address, not the counter PC's, so `staff_computers.txt` cannot tell
them apart: restrict `/staff/` and `/api/staff/` to the counter PCs at the proxy instead.

## Additions beyond the proposal
- **University staff** — every employee on the university list can order. Those with a university email sign up like students; the admin tool creates accounts for those without one.
- **Hidden staff terminal** — no public page links to it, and with `config/staff_computers.txt` filled in it answers "not found" to every computer except the cafeteria's own.
- **Waitlist** — when a fresh item is sold out, anyone ordering can join its waitlist. A cancellation, or staff raising the daily limit, gives the freed portions to the people waiting, first come first served, with a confirmation email. At the cutoff, anyone still waiting gets one "no portion became free" email.
- **Order history and My account** — every finished day is kept in `data/history.txt`; a person's own orders are read into a linked list and merge-sorted newest first. My account shows their details and lets them change their password.
- **Email retry** — an email that fails to send is retried after 1, 5 and 15 minutes, then logged in `data/email_failed.log`. Waiting emails are kept in `data/pending_emails.txt` across restarts. The staff portal shows how many are waiting.

## Pages ("linen and basil" theme)

| Page | Who | What |
|---|---|---|
| `/` | everyone | Landing page: BAUST Khulna photo (save the real one as `web/img/campus-hero.jpg`; a placeholder shows until then), how it works, today's times |
| `/login.html`, `/register.html` | students, teachers, university staff | Log in; register with university email + ID and an emailed code |
| `/menu.html` | students, teachers, university staff | Menu with category tabs, search and sort; dish cards with picture and price; a tray that reserves several dishes at once (all or nothing); waitlist for sold-out dishes |
| `/reservations.html` | students, teachers, university staff | Today's reservations (cancel before the deadline) and your order history from earlier days, newest first |
| `/account.html` | students, teachers, university staff | Your university details; change your password (signs you out on other devices) |
| `/staff/portal.html` | cafeteria staff (not linked from any public page; only from the listed counter computers) | Kitchen (mark ready), menu items (add, price, picture, available today), reservations, packaged stock, settings, undo |

Prices are shown only; people pay at the counter. Staff pictures must be JPEG, PNG or WebP up to 2 MB.
Demo food pictures are simple drawings in `web/img/demo/`. Headings use the serif Cormorant
Garamond (SIL Open Font License, self-hosted in `web/fonts/`) over the system sans. One theme for
everyone, whatever the device's dark-mode setting, modelled on how acclaimed restaurant sites use
colour: a warm linen page (`#F7F3EC`), white surfaces, warm charcoal text, and basil green
(`#1E5B3E`) as the single accent, kept for the brand, buttons and the announcement bar. Labels
are in sentence case. All colours are hex, defined once as tokens at the top of
`web/css/app.css`; `build/contrast_check` and `build/check_hex_colors` keep them accessible and
hex-only.

## Known limits (documented, proposal §10)
- Sessions are kept in memory; a restart logs everyone out.
- One coarse mutex serialises requests — plenty for one cafeteria.
- Real SMTP delivery is untested until an account is provided.

## Publishing to GitHub

The repository (https://github.com/ALI-IOI/CAFETERIA-PRE-ORDER-SYSTEM) is private and holds the
full history, including the original proposal in `docs/proposal/`. Commit on `main` and push it
with `git push`; pull the other developer's work with `git pull`.
