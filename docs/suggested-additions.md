# Suggested additions (not built — waiting for your decision)

Every item is **[optional]**. None of them adds payments, delivery, a mobile app or
advanced analytics (proposal §4 exclusions). Each one either reuses or demonstrates
the four hand-built structures, or makes the existing system sturdier.
Effort: S = an afternoon, M = 1–2 days for one person.

| # | Addition | What it does | Data structure tie-in | Effort |
|---|---|---|---|---|
| 1 | **[optional] Waitlist for sold-out items** | When an item is sold out, a person can join its waitlist. A cancellation before the cutoff gives the freed portion to the first person waiting (auto-reserve + email). | One more hand-built circular queue per fresh item — a second, clearly different FIFO use. | M |
| 2 | **[optional] Email retry queue** | If SMTP fails, the email goes into a retry queue; the minute timer retries it (1, 5, 15 min), then gives up and logs it. Survives restarts via a `pending_emails.txt` file. | Hand-built FIFO queue of notifications. | S–M |
| 3 | **[optional] Staff action log file** | Append-only `staff_log.txt` of every staff action, including "mark ready" (which is not undoable) and undos, with who and when. The undo stack stays the in-memory demonstration; the log is for accountability after restarts. | Shows the difference between a stack (undo order) and a plain append log. | S |
| 4 | **[optional] End-of-day summary for staff** | One table per day: reserved / collected / expired / cancelled portions per item, built by traversing the reservation list and `history.txt`. Helps the cafeteria see waste (expired food). Deliberately just counts, not "analytics". | Linked-list traversal; O(n) explained in the report. | S |
| 5 | **[optional] Bangla / English toggle** | A language switch on the student and teacher pages; text comes from a small string table; the choice is remembered in the browser. Staff portal stays English unless you want both. | — (usability) | S–M |
| 6 | **[optional] More admin-tool commands** | `cafeteria_admin list-staff`, `reset-password <login>`, `remove-staff <login>`. Today the tool can only add staff. | Linked-list delete on the user list. | S |
| 7 | **[optional] Resend code / forgot password for students** | "Resend verification code" button, and password reset by emailed code (same code mechanism as registration). | — (completes the account flow) | S–M |
| 8 | **[optional] Printable kitchen sheet** | A print view of each cooking queue in FIFO order (name, R-code, portions), skipping cancelled entries, for the cook or the counter. | Traverses the circular queue front to rear. | S |
| 9 | **[optional] Accessibility follow-ups** | Mark invalid inputs with `aria-invalid`, and do a recorded NVDA screen-reader pass before the demo (see `docs/accessibility-review.md`). | — | S |
| 10 | **[optional] HTTPS deployment notes + script** | A ready-to-use reverse-proxy config (e.g. Caddy) and a start script with `--behind-https`, so the deployed site uses HTTPS and secure cookies (proposal §10). | — | S |
| 11 | **[optional] Faster login lookup** | Proposal §13 notes that finding a user is O(m) and that a hash table would help "but is outside the four chosen structures". This would add a hand-built hash table. **It changes the project's four-structure scope**, so only if the instructor welcomes it. | Hand-built hash table with chaining (uses linked lists). | M |

## Recommended if you pick only two
- **#1 Waitlist** — the strongest DSA addition: a second queue with a different purpose, easy to demo.
- **#2 Email retry queue** — it fixes a real weakness. Today a failed SMTP send is only logged, so the person never gets that email.

Tell me which numbers to build (or "none"). I won't add any of them on my own.
