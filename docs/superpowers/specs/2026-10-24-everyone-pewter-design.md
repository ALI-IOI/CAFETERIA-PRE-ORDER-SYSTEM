# Everyone at the university, a hidden staff terminal, and the eggshell-and-pewter theme

**Date:** 2026-10-24 · **Status:** approved in chat, awaiting review of this written spec
**Builds on:** `2026-10-23-industrial-redesign-design.md` (pages, tray, staff menu items)

## What the user asked for

1. "The website should be for all students and uni staff, from the VC to the bus driver."
2. "Only the cafeteria staff should be able to see the staff terminal."
3. The dark theme looked bland. Take inspiration from blankedstudios.com and keep the green.
   After three mockups the user chose an eggshell page, cream panels, deep green, and warm
   brushed pewter instead of the cool stainless ("the SS looks bad, replace that with something good").
   Eggshell is the **only** theme: no dark mode.

## Decisions taken in chat

| # | Decision | User's answer |
|---|---|---|
| E1 | How people without a university email get an account | Self sign-up for anyone with a university email; the admin tool creates the account (with a personal email) for those without one |
| E2 | How hidden the staff terminal is | No links anywhere public, **and** "not found" for staff pages and APIs unless the request comes from a listed cafeteria computer |
| E3 | Theme | Eggshell and pewter, deep green, one theme for everyone |

## Part 1 — everyone at the university can order

**New account type.** `Role::Employee`, stored as `employee`, shown as "University staff". It covers
every non-teaching employee (VC's office, registrar, guards, cleaners, bus drivers). Cafeteria staff
remain `Role::Staff` ("cafeteria staff"); the two never mix.

**University list.** `config/university_list.csv` keeps its columns (`id,email,role,name,department`).

- `role` accepts `student`, `teacher` or `employee`.
- `email` may be blank **only** on `employee` rows. A blank email on a student or teacher row is a
  load error naming the line.
- [CONFIRM: which people the university marks as `employee`, and their IDs — the real list comes from the university.]

**Self sign-up** is unchanged: university email + ID + emailed code. It works for any row with an
email, including employees. A row with a blank email cannot self-register: when someone enters
that ID, registration is refused with "You have no university email. Ask the cafeteria office to
create your account." (no code is sent).

**Admin-created accounts.** New command:

```
cafeteria_admin [--data DIR] add-person <university ID> <email>
```

- The ID must be an `employee` row with a blank email in the university list.
- `<email>` is any valid address (it does not have to be on the university domain), lower-cased,
  and must not already be used by another account or another row of the list.
- The starting password is read twice from standard input, exactly like `add-staff`
  (8–128 characters, the same rule as everywhere). The account is active at once.
- Refused while a live server holds `data/server.lock` (same rule as `add-staff`).
- The person logs in with that email and changes the password on My account.

**Rules.** Employees follow the same rules as teachers: per-person limit, cutoff, cancellation
deadline, waitlist, and every email (confirmation, ready, reminders, expiry).

**Pages.**

- My account and the menu-page badge show "University staff" and the department (no Student ID).
- Copy that says "students and teachers" becomes "students, teachers and university staff"
  (landing page, register page, README).
- Access checks: `Access::User` means student, teacher **or** employee.

**Data structures.** No new structure. The users linked list and the university list gain the new
role value; old `users.txt` and `university_list.csv` files load unchanged.

## Part 2 — only cafeteria staff see the staff terminal

**No public links.** Remove the "Staff portal" links from `index.html` (top bar and footer) and
`login.html`. No page a student or employee can reach links to `/staff/`. Only the staff pages
themselves link to each other.

**Counter computers.** New file `config/staff_computers.txt`: one IP address per line (IPv4 or
IPv6, exact match), `#` comments and blank lines allowed. A malformed line stops the server at
start-up with the line number. A missing file counts as an empty list.

- [CONFIRM: the cafeteria counter computers' IP addresses — they must be fixed, not handed out afresh each day.]
- **When the list has entries**, a request for any path starting `/staff/` or `/api/staff/` from an
  address not on the list gets **exactly the same response as a path that does not exist**: status 404
  and the same body. Nobody can tell the terminal exists. The check runs in the pre-routing handler,
  before login, the page guard or the body is read.
- **When the list is empty**, nothing is blocked (links are still hidden). The server prints at
  start-up: `NOTICE: config/staff_computers.txt is empty - the staff terminal is reachable from any computer.`
- The list is the only allow-list. The server's own computer (`127.0.0.1`, `::1`) is **not** allowed
  automatically; add it to the list if the server PC is also a counter PC. This is what lets tests prove the refusal.
- **Limit, documented in the README:** behind a reverse proxy the server sees the proxy's address, so
  the list must then be enforced at the proxy instead.

## Part 3 — the eggshell-and-pewter theme

One stylesheet (`web/css/app.css`) for every page, the staff portal included. The
`@media (prefers-color-scheme: dark)` block is removed. `:root { color-scheme: light; }` keeps form
controls light on devices set to dark mode.

**Colours (tokens on `:root`).**

| Token | Value | Use |
|---|---|---|
| `--page` | Eggshell `#EFE9D9` | Page background |
| `--panel` | Cream `#FAF7EF` | Cards, dish tiles, tray, tables |
| `--pewter` | Pewter `#D4CCB8` | Top bar and hero (a soft top-to-bottom sheen, `#DDD6C3` → `#D4CCB8`) |
| `--pewter-dark` | Dark pewter `#6E685A` | Outlines, "in tray" button, the brushed rule under headings, borders on the page |
| `--line` | Hairline `#857F70` | 1px grid lines and borders on cream panels |
| `--well` | `#E6DFCC` | Inset areas (search box background, empty states) |
| `--ink` | Graphite `#16191C` | Text |
| `--ink-soft` | `#4A463D` | Secondary text |
| `--accent` | Deep green `#127A4B` | Announcement bar, main buttons, selected tab, focus ring |
| `--accent-ink` | `#FFFFFF` | Text on deep green |
| `--accent-text` | Deep green `#0F5C3A` | Green text and links (brand word, "Join waitlist") |
| `--danger` / `--danger-tint` | `#A3241C` / `#F6DED8` | Errors |

The cool stainless (`#C8CDD2`), steel and concrete tokens are removed.

**Contrast (WCAG 2.2; text 4.5:1, borders and focus 3:1)**, all checked by
`tools/contrast_check.py`, which drops its dark pairs and gains these:

| Pair | Ratio |
|---|---|
| Ink on eggshell / cream / pewter / well | 14.6 / 16.5 / 11.0 / 13.3 |
| Soft text on eggshell / cream / pewter | 7.8 / 8.8 / 5.9 |
| White on deep green `#127A4B` | 5.4 |
| Green text `#0F5C3A` on eggshell / cream / pewter | 6.6 / 7.5 / 5.0 |
| Danger on eggshell / cream / danger tint | 6.1 / 6.9 / 5.8 |
| Dark pewter border on eggshell / cream | 4.6 / 5.2 |
| Hairline on cream (borders) | 3.7 |
| Focus ring `#127A4B` on eggshell | 4.4 |

`#127A4B` is never used for text on eggshell (4.43:1, just under 4.5); green text always uses `#0F5C3A`.
The hairline `#857F70` is only used on cream panels (3.3:1 on eggshell is too close to the line); borders
directly on the page use dark pewter.

**Structure borrowed from Blanked Studios.**

- **Announcement bar** in deep green across the top of the landing, menu, reservations and account
  pages: `Orders open until 10:00 · pay at the counter`, or `Orders closed for today · back tomorrow`
  after the cutoff. Times come from `/api/public/info`. It is a `role="note"` element, not a live region.
- **Hairline dish grid.** Dish tiles share 1px `--line` borders with no gaps between them
  (the grid draws the outer left and top edges, each tile its right and bottom).
- **Square corners.** `--radius: 0` everywhere.
- **Labels** (category, "New today", "Sold out", table headings, nav links): 0.75rem, uppercase,
  0.14em letter spacing. Headings keep the self-hosted Oswald; no new font.
- **Spotlight** behind every dish picture: a radial glow from white at the centre to `#E0D9C6`
  at the edge, like Blanked's lit product shots. Sold-out dishes show it at 60% opacity.
- **Brushed rule** under page headings: 2px, dark pewter fading to pewter.

**Unchanged:** page layouts and flows from the industrial redesign (tabs, search, sort, tray,
waitlist, staff panels), the BAUST Khulna photo placeholder, and `textContent`-only rendering.

## Testing (each test seen failing first)

**Unit**

- `employee` role parses and prints; `users.txt` and `university_list.csv` with employee rows round-trip.
- Blank email accepted on an employee row; refused (load error with the line number) on a student or teacher row.
- Self sign-up works for an employee row with an email; refused for a row with a blank email.
- `addPerson` (the core behind `add-person`): accepts a blank-email employee row with a personal email;
  refuses a student row, a row that already has an email, an ID not in the list, an email already in use, a weak password.
- Employees reserve within the same per-person limit as teachers.
- `staff_computers.txt` loading: addresses, comments, blank lines, a malformed line refused with its line number, missing file = empty.

**End to end** (the E2E server is started with a `staff_computers.txt` that does **not** list 127.0.0.1)

- `/staff/login.html`, `/staff/portal.html`, `/api/staff/login` and `/api/staff/overview` answer with the
  same status and body as `/no-such-page` and `/api/no-such-route`.
- After a restart with 127.0.0.1 on the list, staff log in and use the portal as before
  (the existing staff checks run in this phase).
- With an empty list, the start-up notice is printed and the staff pages open.
- The landing, login, register, menu, reservations and account pages contain no `/staff/` link.
- An employee registers with a university email and reserves; an employee added with `add-person`
  (personal email) logs in, reserves and receives the confirmation email in the outbox.
- `admin_tool` suite: `add-person` success and each refusal, and refusal while the server holds the lock.

**Look and feel**

- `python tools/contrast_check.py`: every pair above passes, no dark pairs remain.
- Browser check of every page at desktop and 375 px phone width, with the browser set to dark mode
  (the page must stay eggshell, form controls light), no sideways scrolling.

## Out of scope

- Password reset by email for people who forget their password (no such feature exists today).
- Address ranges (CIDR) in `staff_computers.txt`; exact addresses only.
- SMS or any channel other than email.
- A real campus photo (still the placeholder until the user sends one).
