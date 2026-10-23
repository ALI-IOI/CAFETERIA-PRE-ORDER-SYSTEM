# Industrial redesign — design spec

**Date:** 2026-10-23 · **Status:** for review · **Owner:** Mohammad Ali Gazi, Ayon Das

## Purpose

Make the cafeteria site look deliberate for the course demo and easier to browse,
without changing the rules the proposal defines or the four hand-built data
structures underneath. References chosen by the team:

| Reference | Taken from it | Left out |
|---|---|---|
| Dribbble "Foodly Kitchen" (restaurant / food delivery) | Full-menu page with category filter, search, sort; product cards with big photos; a tray for several dishes | Cart checkout, payment, delivery, star ratings, favourites (proposal §4 excludes payment and delivery; ratings would be invented data) |
| Astra "Furniture Shop 04" | Industrial look: condensed uppercase headings, steel-grey panels, objects shown large on plain tiles | Shop-specific sections (brand logos, sale badges) |
| Astra "Brandstore 02" | Landing layout: full-width photo hero, big headline, two buttons, tiles below | Promo/sale tiles, product grid on the landing page |

## Decisions (agreed in chat)

- **D-R1** Plain HTML/CSS/vanilla JavaScript, no framework (proposal §1 "no framework").
- **D-R2** Prices are shown in taka and paid in cash at collection. No payment step anywhere.
- **D-R3** Staff add and edit menu items (name, category, fresh/packaged, price, picture, available today). Items are **hidden, never deleted**: reservations refer to items by their array index.
- **D-R4** The tray reserves **all or nothing**: if any dish breaks a rule, nothing is reserved and the message names the dish.
- **D-R5** Landing page = hero + "How it works" (3 steps) + "Today's info" strip. No featured-dish grid.
- **D-R6** The BAUST Khulna photo is a **placeholder**: `web/img/campus-hero.svg`, a labelled stainless panel shown behind an optional `web/img/campus-hero.jpg`. The team replaces it by saving the real photo as `campus-hero.jpg`. No university logo is used.
- **D-R7** Demo food pictures are simple **SVG illustrations drawn for this project** (bowl, chips packet, can, bottle), used until staff upload real photos. No stock or generated photos.
- **D-R8** Oswald (SIL Open Font License) is downloaded once and self-hosted in `web/fonts/` with its licence file; the server's CSP stays `default-src 'self'`.
- **D-R9** Placeholders to confirm: menu array capacity `[CONFIRM: 32 items]`, picture upload limit `[CONFIRM: 2 MB]`.

## Section 1 — Functionality

### 1.1 Menu items managed by staff
- `MenuItem` gains `priceTaka` (int, 0–9999), `imageFile` (server-chosen file name or empty), `available` (bool).
- `menu.txt` gains three fields: `category|name|kind|stock|dailyLimit|priceTaka|imageFile|available`. Old 5-field lines still load (price 0, demo picture, available).
- Staff portal "Menu items" panel: add item; edit price, availability, picture. Name and kind are fixed after creation (they label existing reservations).
- An unavailable item disappears from the student menu; `reserve` and `joinWaitlist` refuse it with a new `Result::NotAvailable`. Existing reservations, cooking queues and reminders for it carry on unchanged. Its waitlist **pauses**: nobody is promoted while it is unavailable, people keep their place, and the normal cutoff email still closes the list.
- Adding an item when the array is full is refused with a clear message (array capacity is the hand-built limit).
- Every menu change is a staff action on the undo stack where it can be undone (price, availability); adding an item and changing a picture are not on the undo stack (like "mark ready").

### 1.2 Picture upload (staff only)
- `POST /api/staff/items/image` (multipart, one file). Accepted: JPEG, PNG, WebP, recognised by the file's first bytes, not its name or claimed type. Max `[CONFIRM: 2 MB]`.
- Saved as `data/images/item-<index>-<random hex>.<ext>`; the previous picture file is deleted. Served read-only at `/images/<file>`.
- Every other route keeps its 4 KB body limit; only this route accepts the larger body.

### 1.3 Menu page (replaces `home.html` with `menu.html`)
- Category tabs built from the menu's categories (Fresh food, Chips, Soft drinks, Water, …), a search box (name contains), sort by name / portions left / price.
- Card: big picture, name, price, portions left with a bar (fresh) or "in stock / out of stock" (packaged), then **Add to tray** — or **Join waitlist** / "You are #N" when sold out (waitlist behaviour unchanged).
- Filtering and sorting happen in the browser on the data from `/api/menu`.

### 1.4 Tray
- Kept in the browser (`sessionStorage`) until reserved; shows each dish, portions (within the per-person limit), line prices and a total "to pay at the counter".
- `POST /api/reservations/batch` with `items=<index>:<qty>,<index>:<qty>`. Under the one lock the server first checks every line against the normal rules, counting earlier lines of the same tray (cutoff, availability, sold out, per-person limit, cooking-queue room); if all pass it makes every reservation, otherwise none.
- One confirmation email lists every dish, its reservation code and the total. The single-dish `POST /api/reservations` stays for the console demo and tests.

### 1.5 Landing page (`index.html`, public)
- Hero with the campus photo placeholder, headline "Fresh food, reserved before {cutoff}", **Log in** and **Create account**. Login moves to `login.html`.
- "How it works": 1 Reserve before the cutoff · 2 We cook and email you when it's ready · 3 Collect while it's fresh.
- "Today's info" strip from a new public `GET /api/public/info`: cutoff, cancellation deadline, freshness window, server time. No personal data, no counts.

### 1.6 Unchanged
All proposal rules, statuses, the waitlist, email retry, staff portal features, security measures, persistence and the four data structures.

## Section 2 — Visual system ("green and stainless")

| Token | Value | Used for |
|---|---|---|
| Graphite | `#16191C` | Text, header bar, secondary buttons |
| Stainless | `#C8CDD2` | Hero and feature panels (silver stainless steel) |
| Steel | `#6B737B` | Hairlines, icons, borders |
| Light steel | `#E2E5E8` | Picture wells, bars, tabs |
| Concrete | `#F1F2F3` | Page background, cards |
| Machine green | `#1F6B45` | The one main button per screen, accent rules, portions bar |
| Deep green | `#0F5C3A` | Green text on stainless |

- Contrast (measured): graphite on concrete 15.7:1; graphite on stainless 11.0:1; white on machine green 6.5:1; machine green on concrete 5.8:1; deep green on stainless 5.0:1; accent rule on stainless 4.0:1 (non-text, needs 3:1). Green text never sits directly on stainless in the lighter shade.
- Type: Oswald 500, uppercase, slight letter-spacing for headings and card names; system UI font for body; tabular figures for prices and counts.
- Shape: flat panels, 2–4 px corners, 1 px steel hairlines, no soft shadows. Header: graphite bar with a machine-green rule.
- Dark mode (follows the device setting): graphite page, dark steel panels, light text; the green is lightened as needed to keep the same contrast levels.
- Accessibility kept from the current pages: skip link, labels on every control, visible focus ring, 44 px targets, status text not colour alone, reduced-motion respected, no horizontal scroll at 375 px.
- Staff portal: same system, denser "control panel" layout; the queues and the undo stack are shown as steel panels.

## Testing
- Unit (test-first): menu fields load/save incl. old format; `NotAvailable` for reserve and waitlist; batch all-or-nothing (each rule, counting earlier tray lines, nothing written on failure, one email); image signature check (JPEG/PNG/WebP accepted, renamed text/HTML/oversized refused); item add at capacity refused.
- E2E: public info without login; landing and menu pages served; staff adds an item with price and picture, student sees it; unavailable item refused; tray reserve all-or-nothing; uploaded picture served, non-image upload refused, student cannot upload.
- Browser check of every page at desktop and 375 px, keyboard pass, contrast check of the final CSS.

## Out of scope
Payment, delivery, ratings/reviews, favourites, analytics, a mobile app, deleting menu items.
