# ADR-001: Server-side architecture

**Status:** Proposed
**Date:** 2026-10-23
**Deciders:** Ayon Das, Mohammad Ali Gazi (course team); course instructor for scope

## Context

The proposal (sections 6–8, 10, 11) asks for a C++ server whose core is four hand-built
data structures (array, linked list, queue, stack), wrapped by an HTTP layer, a minute timer,
file storage and email. Third-party code is allowed only for web, email and security.
Constraints that shape the design:

- Correctness and readability of the DSA code matter more than throughput (one cafeteria,
  tens to a few hundred reservations per day).
- Several HTTP requests and the timer thread touch the same structures at once (§6).
- A restart must restore menu, users and reservations and rebuild the queues (§8, §12).
- Time-based rules (10:00 cutoff, hourly reminders, 7-hour freshness) must be testable
  without waiting real hours.
- Toolchain on the dev machine: MinGW-w64 g++ 13.1 (UCRT, posix threads), CMake, Ninja.
  No libsodium / OpenSSL / libcurl **headers** are installed. Windows ships `curl.exe` 8.21
  with `smtp`/`smtps` support.

## Decision

### Layers and request flow

```
browser (HTML/CSS/JS, no framework)
   │  form-encoded POST / GET, session cookie (HttpOnly, SameSite=Strict)
   ▼
server/  cpp-httplib routes ── validate input ── look up session ── check role
   │        lock g_stateMutex  (one std::mutex for the whole AppState)
   ▼
core/    Cafeteria rules (reserve, cancel, markReady, markCollected, undo, timerTick)
   │        uses only ds/ structures; returns a NotificationList (hand-built linked list)
   ▼
ds/      MenuArray · ReservationList · ResQueue (circular) · ActionStack   ← no STL containers
   │
storage/ save changed state to data/*.txt (write temp file, then rename)
   │        unlock g_stateMutex
   ▼
email/   send the collected NotificationList OUTSIDE the lock (SMTP is slow)
```

- **One coarse mutex.** Every route handler and every timer tick takes `g_stateMutex`
  for the whole read-modify-save sequence. With one cafeteria's load, a single lock is
  simpler to reason about and to explain in a viva than fine-grained locks, and removes
  any chance of lock-ordering deadlock.
- **Notifications leave the lock as data.** Core functions never send email; they append
  `Notification` nodes. The caller sends them after unlocking, so a slow SMTP server
  cannot block other requests.
- **Injectable clock.** `core` reads time only through `Clock*`. Production uses
  `SystemClock`; unit tests use `FakeClock`; the server accepts `--fake-clock` for the
  end-to-end test (adds a localhost-only `/api/dev/advance` route, off by default).
- **Timer thread.** `std::thread` that wakes every 60 s (`--tick-seconds` overrides for
  tests), locks, calls `timerTick`, saves, unlocks, sends email.
- **Queues hold `Reservation*`.** Nodes are never freed while a queue may reference them:
  cancel/collect/expire only change `status`; queues skip non-matching statuses when an
  entry reaches the front (lazy deletion, as §7 specifies for cancelled entries).
  Nodes are deleted only at day rollover, and only nodes in a final state
  (Collected / Cancelled / Expired) that are in no queue.

### Persistence and restart recovery

Text files in `data/`, one record per line, `|`-separated (inputs containing `|` or
newlines are rejected by validation, so no escaping is needed):

| File | Contents |
|---|---|
| `menu.txt` | index, category, name, kind, stock, dailyLimit, reservedToday |
| `settings.txt` | key=value (cutoff, cancel deadline, freshness, reminder interval, per-user limit) |
| `users.txt` | email/login, university ID, role, name, department, password hash, verified, failed count, locked-until |
| `reservations.txt` | id, user, item, qty, reservedAt, status, readyAt, lastNoticeAt, day |
| `history.txt` | append-only archive of previous days' reservations |

Every mutating request saves before it unlocks (atomic temp-file + rename, so a crash
mid-write leaves the previous file intact). On start-up: load files → array and lists;
rebuild each cooking queue from `Reserved` reservations in list (= arrival) order; rebuild
the reminder queue from `Ready` reservations ordered by next-due time (insertion sort on
a temporary pointer array, then enqueue); action stack starts empty (§8). Ready
reservations whose freshness window ended while the server was down are expired on the
first tick.

### Library choices (needs team confirmation — see plan, Decision D1)

| Need | Option A (recommended) | Option B |
|---|---|---|
| HTTP | cpp-httplib single header (download required) | — (writing an HTTP server is out of scope) |
| Password hashing + random tokens | libsodium prebuilt MinGW package (download): `crypto_pwhash_str` (Argon2id), `randombytes_buf` | Windows CNG `bcrypt.dll` (already on the machine): PBKDF2-HMAC-SHA256 + `BCryptGenRandom` |
| SMTP | run Windows `curl.exe` (libcurl inside) via `CreateProcess`, no shell | libcurl dev package (download) and link it |
| HTTPS | reverse proxy (e.g. Caddy) in front when deployed | cpp-httplib + OpenSSL dev package |

## Options considered for concurrency

### Option A: one global mutex (chosen)
| Dimension | Assessment |
|---|---|
| Complexity | Low |
| Scalability | Enough for one cafeteria; requests serialise |
| Team familiarity | High — `std::lock_guard` around each handler |

### Option B: one mutex per structure
| Dimension | Assessment |
|---|---|
| Complexity | High — reserve touches array + list + queue, needs lock ordering |
| Scalability | Better under heavy load (not needed) |
| Team familiarity | Low; deadlock risk |

## Consequences

- Easier: proving the DSA logic in a console program first; deterministic tests with a fake clock;
  explaining every request as "lock → structures → save → unlock → email".
- Harder: throughput is bounded by one lock (acceptable at this scale).
- Revisit: the reminder-queue ordering assumption (plan, Decision D7) if staff change the
  interval mid-day; HTTPS when actually deployed.

## Action items

1. [ ] Team confirms library option (D1) and the open decisions in the implementation plan.
2. [ ] Implement per `docs/superpowers/plans/2026-10-23-cafeteria-pre-order-system.md`.
