# University Cafeteria Food Pre-Order System (BAUSTK, DSA I)

A C++ web system in which everyone at the university (students, teachers and university
staff, from the VC's office to the bus drivers) reserves freshly cooked food before a
cutoff, staff mark food ready, and everyone gets emails. **All reservation logic runs on
four hand-built data structures** — no STL containers for them (checked by a build test).

It runs locally: build it (below) and start the demo server as `demo/README.md` describes. The
student IDs in the sample lists and tests are the team's real university IDs; every email uses
the reserved test domain `uni.example`.

## ▶ Live demo

The system has two login terminals. Both are live, with the demo accounts listed on each page:

| Terminal | Who logs in there | Live page | Demo login |
|---|---|---|---|
| **Public login** | Students, teachers and university staff | [ali-ioi.github.io/CAFETERIA-PRE-ORDER-SYSTEM/login.html](https://ali-ioi.github.io/CAFETERIA-PRE-ORDER-SYSTEM/login.html) | `ali@uni.example` / `demo-student-pass` (more on the page) |
| **Staff login** | Cafeteria staff only | [ali-ioi.github.io/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html](https://ali-ioi.github.io/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html) | `demo.staff` / `demo-staff-pass` |

The real C++ server runs in your browser, compiled to WebAssembly: each browser gets its own demo
cafeteria, with a clock that starts at 08:30, an inbox for the emails and a reset button. Open both
terminals in two tabs to reserve as a student and mark food ready as staff. How it works and how to
build it: `web-demo/README.md`.

| Structure | Where | Used for |
|---|---|---|
| Array | `src/ds/menu_array.*` | Menu, packaged stock, fresh portions (O(1) by index) |
| Linked list | `src/ds/reservation_list.*`, `src/auth/users.*` | Reservations (tail insert O(1), search/cancel O(n)); user accounts; university list; sessions; order history (merge sort, newest first, O(n log n)) |
| Circular queue | `src/ds/res_queue.*` | One cooking queue per fresh item (FIFO "ready" emails, cancelled entries skipped); the reminder queue (front = next reminder due) |
| Array stack | `src/ds/action_stack.*` | Staff action history; "undo the latest action" (LIFO) |
| Circular queue (addition) | `src/ds/wait_queue.*` | Waitlist per sold-out fresh item: a freed portion goes to the first person waiting |
| Linked queue (addition) | `src/email/mailer.*` | Emails that failed to send, retried after 1, 5 and 15 minutes |
| Fixed array (addition) | `src/core/coupons.cpp` | Coupons, up to 64, in the order staff made them; lookup by code is a linear search, O(c); "once a day per person" walks today's reservations, O(n) |
| Insertion sort (addition) | `src/core/staff.cpp` (`staffOrdersSorted`) | The staff order list: today's reservations, newest token first, each order's dishes in the order they were reserved |
| Direct-address array (addition) | `src/core/tokens.cpp` | Order tokens: today's reservations grouped by token number in one pass, O(n + t); the token board's ready column ordered by when each token became ready (insertion sort) |

Rules live in `src/core/` (`cafeteria.cpp`, `staff.cpp`), storage in `src/storage/`,
security in `src/auth/`, email in `src/email/`, the HTTP server in `src/server/`, pages in `web/`.
Design: `docs/architecture/ADR-001-server-architecture.md`. The working plans were removed on 2026-11-01; they are in the git history (commit 08754aa and earlier).
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
| `build/check_hex_colors` | Fails if any colour in `web/` is not written in hex (run by `ctest`) |
| `build/check_pages` | Fails if a page or the stylesheet breaks the static page rules (run by `ctest` as `page_rules`) |

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
   - `config/staff_computers.txt` — the cafeteria counter computers' IP addresses (one per line). Once it lists any, every other computer gets "not found" for the staff terminal; while it is empty, only the server computer itself can open it
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
- **Removing menu items** — cafeteria staff can add items and remove ones they no longer sell. A removed item leaves the menu and the staff lists but keeps its place in the menu array, so reservations and history still name it; it cannot be removed while it has reservations or a waitlist today, and Undo brings it back.
- **Order history and My account** — every finished day is kept in `data/history.txt`; a person's own orders are read into a linked list and merge-sorted newest first. My account shows their details and lets them change their password.
- **Email retry** — an email that fails to send is retried after 1, 5 and 15 minutes, then logged in `data/email_failed.log`. Waiting emails are kept in `data/pending_emails.txt` across restarts. The staff portal shows how many are waiting.
- **Order tokens and the token board** — as at a restaurant counter, every order gets a token number for the day (001, 002, …): a single reservation, a whole tray (all its dishes share one token) or a waitlist promotion. The token is in the confirmation and "ready" emails and on My reservations. The public **token board** (`/board.html`) shows which tokens are ready at the counter, in the order they became ready, and which are still cooking, with token numbers only (no names). At the counter, staff type the token (or press its button) to hand over every ready dish of it; each dish handed over is one step on the undo history. Tokens start again at 001 each morning and continue after a restart (the token is the 12th field of `reservations.txt` and `history.txt`; older 9- and 11-field lines still load).
- **Teachers' specials** — cafeteria staff can make any fresh dish a teachers' special (Menu items → "Who can order", or when adding a dish). Only teachers see it on the menu and can reserve it or join its waitlist; students and university staff never see it, and the server refuses their orders for it. Each change is one step on the undo history. Saved as a 10th `menu.txt` field (older 5-, 8- and 9-field lines still load).
- **Orders placed** — the staff portal lists every order of the day, newest token first: who placed it (name, group, email), when, each dish with its price and status, and what is left to pay at the counter after a coupon.
- **Coupons** — staff make a coupon for an occasion (Pohela Boishakh, Teachers' Day, an exam week…): a code of 3–16 letters and digits, a percentage (1–100 %) or a fixed amount (1–9999 taka) off the order, who may use it (everyone, students, teachers or university staff), an optional first and last day and an optional most number of orders. They can switch a coupon off or remove it. People see the coupons they may use today in the tray, apply one with a code, and see the discount before they reserve; the server checks it again when the tray is reserved. A person can use a coupon once a day; the discount is shared across the order's dishes and comes off what is paid at the counter (no money is taken online). Cancelling every dish of an order gives the coupon use back. Coupons are kept in `data/coupons.txt`; each reservation keeps the price it was ordered at, the coupon and its share of the discount (fields 13–15 of `reservations.txt`; older lines take today's price). Coupon changes are not on the undo history.

## Pages ("chili and rice" theme)

| Page | Who | What |
|---|---|---|
| `/` | everyone | Landing page: a chili-red field with the "Now serving" token stamp, how it works (with the drawn table of plates, `web/img/hero-food.svg`, made for this site), today's times as stamps |
| `/board.html` | everyone | The token board for the screen at the counter: ready tokens (first ready first) and tokens still cooking; numbers only, updates every 5 seconds |
| `/login.html`, `/register.html` | students, teachers, university staff | Log in; register with university email + ID and an emailed code |
| `/menu.html` | students, teachers, university staff | Menu with category tabs, search and sort; dish cards with picture and price; a tray that reserves several dishes at once (all or nothing), with today's coupons and a coupon field; waitlist for sold-out dishes; teachers also see teachers' specials |
| `/reservations.html` | students, teachers, university staff | Your tokens today (cooking or ready), today's reservations with their token (cancel before the deadline) and your order history from earlier days, newest first |
| `/account.html` | students, teachers, university staff | Your university details; change your password (signs you out on other devices) |
| `/staff/portal.html` | cafeteria staff (not linked from any public page; only from the listed counter computers) | Token counter (hand over by token), kitchen (mark ready), orders placed today, menu items (add, remove, price, picture, available today, everyone or teachers only), coupons, reservations, packaged stock, settings, undo |

Prices are shown only; people pay at the counter. Staff pictures must be JPEG, PNG or WebP up to 2 MB.
Fresh dishes use simple drawings in `web/img/demo/`. The packaged snacks and drinks the cafeteria sells (bought from the official importers) have product photos in `web/img/products/` (600 × 400 PNG, background removed), named in `seed/menu.txt` as `product-*.png`; staff can still upload their own picture. Starting prices are Bangladesh retail prices read on chaldal.com and arogga.com on 2026-11-09; items whose pack price could not be confirmed show "Price at counter" until staff set it. The online demo starts each packaged item at 24 in stock. Headings use the serif Cormorant
Garamond (SIL Open Font License, self-hosted in `web/fonts/`) over the system sans. One theme for
everyone, whatever the device's dark-mode setting, inspired by the restaurant site Délice (a red
field, cream type, a light high-contrast serif and round stamps with a double ring) and set in the
cafeteria's own spice colours: a rice-cream page (`#FBF1E4`), tamarind-brown text (`#2A1410`), a
chili-red header band, home field and main buttons (`#B42318`), turmeric (`#F2B531`) only as a
filled mark with tamarind text on it ("Ready", the tray count), and curry-leaf green (`#2F6B3C`) and
mint (`#9ED7C3`) as second voices (ready stamps, quiet panels). The order token is shown as a round
stamp, the one round shape in an otherwise square-cornered design. Labels
are in sentence case. All colours are hex, defined once as tokens at the top of
`web/css/app.css`, which is one ordered design system: tokens (colours, a 6-step type scale, an
8-step spacing scale, motion), base, layout, components, states, page rules, motion. Every list
has its loading, empty and error state, and every button that waits for the server is locked
until it answers. `build/contrast_check`, `build/check_hex_colors` and `build/check_pages` (one
title, labelled fields, no inline styles or scripts, sizes only from the scale, no motion over
300 ms) keep the pages to these rules.

## Known limits (documented, proposal §10)
- Sessions are kept in memory; a restart logs everyone out.
- One coarse mutex serialises requests — plenty for one cafeteria.
- Real SMTP delivery is untested until an account is provided.

## Publishing to GitHub

The repository (https://github.com/ALI-IOI/CAFETERIA-PRE-ORDER-SYSTEM) is public and holds the
full history, including the original proposal in `docs/proposal/`. Commit on `main` and push it
with `git push`; pull the other developer's work with `git pull`.

## Licence

MIT (see `LICENSE`). Included third-party code keeps its own licence: cpp-httplib (MIT, `third_party/httplib/LICENSE`), libsodium (ISC, `third_party/libsodium/LICENSE`) and the Cormorant Garamond font (SIL Open Font License, `web/fonts/OFL.txt`).
