# Rebuild of the web pages, same look — design

Date: 2026-10-24. Status: approved in conversation, section by section.

## Goal

Rebuild all eight pages in `web/` to the standard of the design skills (`ui-ux-pro-max`,
`impeccable`), keeping the "linen and basil" look the team chose. This is a DSA course
project: the C++ server, the hand-built data structures and the API do not change. The pages
stay plain HTML, CSS and JavaScript; the tools and tests stay C++/CMake.

Pages: `/` (home), `/login.html`, `/register.html`, `/menu.html` (with the tray),
`/reservations.html`, `/account.html`, `/staff/login.html`, `/staff/portal.html`.

## Constraints

- No new features: same data, same actions, same API calls. Only layout, components, states,
  wording and motion change.
- No libraries, frameworks, CDNs or build step. Everything self-hosted under the server's CSP
  (`default-src 'self'`).
- Colours hex only (checked by `build/check_hex_colors`); WCAG AA contrast (checked by
  `build/contrast_check` and on the rendered pages).
- Controls at least 44 px tall; no sideways scrolling at any width.
- The end-to-end test stays green: `--page: #F7F3EC`, `--bar: #FFFFFF`, `color-scheme: light`,
  no `prefers-color-scheme` rules, every element ID the scripts use, and the page wording it
  checks ("university staff" on `/`, `/login.html`, `/register.html`; "University staff" in
  `js/api.js`; none of "Student or teacher", "University email" on login, "Student & teacher";
  no `/staff/` link on public pages; staff scripts under `/staff/`).
- Approach: rebuild in place. `web/css/app.css` stays one file, rewritten as an ordered design
  system: tokens, base, layout, components, page rules, motion.

## 1. Visual system

**Colours** (unchanged): page `#F7F3EC`, panel and bar `#FFFFFF`, line `#E2DACD`, field edge
`#8A8174`, well `#EFE9DE`, ink `#1F1D1A`, ink-soft `#5E5850`, accent `#1E5B3E`, accent hover
`#143F2B`, tint `#E3EDE6`, danger `#A3241C` on danger tint `#F6DED8`. Green is the only accent
(brand, main buttons, selected tab, "ready"). No new hues: "few left" is shown in words and by
the bar.

**Type**: headings Cormorant Garamond 600 (self-hosted), body the system sans. One six-step
scale used everywhere: 14, 16, 20, 26, 35 and up to 58 px for the page title (fluid, smaller on
phones). Tabular figures for prices, times, counts and IDs.

**Spacing**: one scale, 4, 8, 12, 16, 24, 32, 48, 64 px.

**Shape**: square corners, 1 px warm lines, no shadows. Signature details: a 3 px green rule on
top of each lead surface (top bar, tray, panels that need attention) and a dotted leader between
a dish's name and its price.

**Layout**: content at most 1180 px wide. Three designed widths: phone (up to 640 px), tablet
(641–1000 px), desktop (over 1000 px).

## 2. Page layouts

- **Home**: full-width campus photo with a white panel (title, one line, Log in / Create
  account, today's status from `/api/public/info`, e.g. "Orders are open until 10:00"); "How it
  works" as three numbered steps with large serif numerals; a strip of today's times (reserve
  before, cancel until, stays fresh, server time).
- **Log in, register, staff log-in**: one centred card on linen. Register shows a two-part
  progress line above its heading (step 1: your details; step 2: the emailed code).
- **Menu**: top row with name, role badge and university ID on the left and open/closed status
  with the closing time on the right; toolbar with category tabs (one sideways-scrolling row on
  phones), search and sort; dish tiles in the line grid (picture; name, dotted leader, price;
  portions left in words and as a bar; portions choice and Add). Sold out: greyed with the
  waitlist offer. Packaged: "in stock, buy at the counter". Tray: sticky right column on
  desktop, the fixed green bar at the bottom on phones and tablets.
- **My reservations**: today's reservations in one table with status chips, a ready
  reservation on a pale green row; earlier days grouped under date headings, newest first (the
  merge-sorted history). Rows become labelled cards on phones.
- **My account**: two cards, details and change password.
- **Staff portal**: sticky left section navigation on desktop, a row of tabs on phones and
  tablets. Kitchen: one panel per fresh dish with the cooking-queue count (first in, first
  out), reserved out of the daily limit as a bar, and Mark next ready. Undo history beside it,
  newest on top, marked as the one Undo reverses. Reservations, menu items, packaged stock and
  settings keep their tables and forms on the new grid.
- **DSA**: the cooking queue, waitlist, undo stack and newest-first history are named in the
  interface where they are used.

## 3. Components and states

| Component | States |
|---|---|
| Buttons: main, outline, danger, text link | normal, hover, pressed, focus, disabled, busy (label changes, e.g. "Reserving…", button locked until the server answers) |
| Fields and selects | empty, filled, focus, invalid (red edge, message under the field via `aria-describedby`), disabled |
| Category tabs | normal, hover, selected, focus |
| Dish tile | available; few left (fewer than 3: "Only 2 left"); sold out (greyed, waitlist offer); on your waitlist ("You're number 2 on the waitlist"); in your tray ("In tray: 2") |
| Tray | empty, has dishes, reserving, done (reservation codes), error (server message, nothing reserved) |
| Status chips | Reserved, Ready, Collected, Cancelled, Expired: word plus style, never colour alone |
| Messages | success, error, info; announced with `role="status"` or `role="alert"` |
| Lists and tables | loading ("Loading…"), empty (sentence plus next step), error (message plus Try again), filled |
| Closed for the day | "Orders are closed" banner; tiles cannot be added; history stays readable |
| Kitchen panel | nothing waiting, waiting (portions waiting and orders in the cooking queue, as the overview sends them), all ready |
| Undo history | empty ("Nothing to undo"), filled (newest on top), undoing |

Wording: sentence case, plain words, no arrows or decorative dots, numbers with units ("৳ 80",
"10:00", "3 portions"); error messages say what happened and what to do next. Scripts keep
their API calls and data; they only gain the markup for these states.

## 4. Motion

- Hover and press: colour and position, 120–150 ms.
- Adding to the tray: count pulses once (200 ms); the new line slides in (180 ms); removing
  fades out.
- Status changes: the row background fades to its new tint over 300 ms.
- Messages and banners fade in over 150 ms. Nothing slides across the page, loops or plays on
  page load.
- Only colour, opacity and transform are animated. `prefers-reduced-motion: reduce` makes every
  change instant. CSS only, with class toggles from the existing scripts.

## Checks and done

- `accessibility` (WCAG 2.2) and `web-quality-audit`, standing in for the uninstalled
  `web-design-guidelines`; a motion review against `impeccable`, standing in for
  `design-motion-principles` / `review-animations`. Fix what they flag.
- C++ checks: `contrast_check`, `hex_colors_only`, `e2e_section12`, and the rest of `ctest`.
- Every page rendered at 375, 768 and 1280 px: no sideways scroll, no tap target under 44 px,
  no text below AA, every state above shown at least once.
- Done: 8/8 `ctest`, no build warnings, checks clean, all pages rendered at all three widths,
  committed and pushed.

Not installed, so not used: `design-taste-frontend`, `interface-design`, `ui-refactor`,
`design-motion-principles`, `animate`, `web-design-guidelines`, `review-animations`.
