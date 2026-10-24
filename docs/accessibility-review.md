## Accessibility Audit: cafeteria web pages
**Standard:** WCAG 2.1 AA | **Date:** 2026-10-23 | **Pages:** index, register, home, reservations, staff/login, staff/portal

### Summary
**Issues found:** 4 | **Critical:** 0 | **Major:** 2 (fixed) | **Minor:** 2 (1 fixed, 1 open)

How it was checked: contrast ratios computed for every colour pair in both themes;
in-browser script for unlabeled controls and touch targets under 44 px; keyboard
Tab-through with focus-ring check; 375 px phone width for horizontal overflow.
Not done: a real screen-reader session (NVDA/VoiceOver) — recommended before the demo.

### Findings

#### Perceivable
| # | Issue | WCAG | Severity | Status |
|---|---|---|---|---|
| 1 | Item names in table rows rendered as small uppercase grey (header style leaked into `tbody th`) | 1.3.1 / readability | Minor | Fixed: header style limited to `thead th` |

#### Operable
| # | Issue | WCAG | Severity | Status |
|---|---|---|---|---|
| 2 | Focus ring on the dark-green top bar was 2.42:1 (light theme) | 2.4.7, 1.4.11 | Major | Fixed: lighter amber ring on the bar, 6.7:1 |
| 3 | Staff portal wider than a 375 px phone: grid column could not shrink, and a visually-hidden table label escaped the scroll box | 1.4.10 Reflow | Major | Fixed: `min-width: 0` on grid items, `position: relative` on `.table-wrap`; page width now 375 px |

#### Understandable
| # | Issue | WCAG | Severity | Status |
|---|---|---|---|---|
| 4 | Error messages are announced (`role="status"`) but the offending input is not marked `aria-invalid` | 3.3.1 | Minor | Open — messages already name the problem in words |

#### Robust
No issues: every input/select/button has a label or `aria-label` (script found 0 unlabeled); status chips carry text, not colour alone; the queue meter has `role="img"` with a text label.

### Color Contrast Check (computed)
| Element | Light | Dark | Required |
|---|---|---|---|
| Body text on page | 14.28:1 | 15.46:1 | 4.5:1 ✅ |
| Secondary text | 7.21:1 | 8.58:1 | 4.5:1 ✅ |
| Links / green text | 7.34:1 | 8.47:1 | 4.5:1 ✅ |
| Button text | 7.91:1 | 10.15:1 | 4.5:1 ✅ |
| Status chips (Reserved/Ready/Expired/Cancelled) | 6.20–6.98:1 | 7.34–7.85:1 | 4.5:1 ✅ |
| Role badge | 5.97:1 | 7.40:1 | 4.5:1 ✅ |
| Top bar text | 10.97:1 | 16.46:1 | 4.5:1 ✅ |
| Input borders (non-text) | 3.68:1 | 3.33:1 | 3:1 ✅ |
| Focus ring on page / on top bar | 4.66:1 / 6.7:1 | 10.27:1 | 3:1 ✅ |

### Keyboard Navigation
| Element | Behaviour |
|---|---|
| Skip link | First Tab stop on every page, jumps to `<main>` |
| Top bar links, Log out | Reachable in order; Log out is a real `<button>` |
| Forms | Native inputs/selects/buttons; Enter submits |
| Registration step 2 | Focus moves to the code field when it appears |
| Staff tables | "Collected"/"Cancel" buttons have labels naming the reservation (e.g. "Mark R-0001 collected") |

### Other checks
- Touch targets: 0 interactive elements under 44 px tall.
- `prefers-reduced-motion` respected (transitions disabled).
- Light and dark themes both follow the system setting.

### Priority fixes remaining
1. **Screen-reader pass** with NVDA on the staff portal before the demo.
2. Mark invalid inputs with `aria-invalid` alongside the message (minor).

---

## Re-check after the industrial redesign (2026-10-24)

Pages: landing `/`, `/login.html`, `/register.html`, `/menu.html`, `/reservations.html`, staff portal.

| Check | Result |
|---|---|
| Contrast, light and dark themes | `tools/contrast_check.py`: 24/24 pairs pass (body text 15.7:1; white on machine green 6.5:1; deep green on stainless 5.0:1; focus rings 5.8:1 / 9.7:1) |
| Labels | In-browser script: 0 unlabeled inputs, selects or buttons on the staff portal (including the new Menu items table: every price, availability and picture control has a visually hidden label naming the item) |
| Touch targets | 0 controls under 40 px except the brand link (38 px, a text link) |
| Reflow at 375 px | Landing, menu and staff portal: page width 375 px, no horizontal scroll |
| Tabs | Category tabs are buttons with `aria-pressed`; the tray is a labelled landmark (`aside` + heading) |
| Status | Tray and menu messages use the existing `role="status"` region; meters have `role="img"` with a text label |
| Pictures | Dish pictures have alt text ("Picture of Tehari"); the decorative thumbnail in the staff table has empty alt because the item name is in the same row |

Still open from the first audit: `aria-invalid` on invalid fields (minor) and a real screen-reader (NVDA) pass before the demo.

## Re-check after the eggshell theme (2026-10-24)

Pages: landing, login, register, menu, reservations, account, staff login, staff portal. One theme
for everyone; the browser was set to dark mode for every check.

| Check | Result |
|---|---|
| Contrast | `tools/contrast_check.py`: 19/19 pairs pass (body text 14.6:1 on eggshell, 17.7:1 on white; white on deep green 5.4:1; green text 6.6:1 on eggshell; field and outline-button edges 4.4:1 or more) |
| Non-text contrast | Every control's edge (inputs, selects, outline buttons, tabs) is deep green #127A4B, 4.4:1 or more. The light stainless lines (#C8CDD2, about 1.5:1) are decoration only: dish-grid lines, table rows, card edges |
| Dark-mode devices | `color-scheme: light`; no `prefers-color-scheme` rules. With dark mode emulated, every page kept the eggshell background and light form controls; 0 elements with a dark background |
| Reflow at 375 px | All 8 pages: no horizontal scroll |
| Announcement bar | `role="note"` (not a live region), text only, same text on every page that shows it |
| Staff links | Public pages contain no link to `/staff/` (checked by the E2E suite) |

Still open from the first audit: `aria-invalid` on invalid fields (minor) and a real screen-reader (NVDA) pass before the demo.

## Phone, tablet and desktop check (2026-10-24)

Every page measured in the browser at 375×812 (phone), 812×375 (phone on its side), 768×1024
(tablet), 1280×800, 1440×900 and 1920×1080 (desktop), logged in as a student-side account and as staff.

| Check | Before | After |
|---|---|---|
| Sideways scrolling | none on any page | none on any page |
| Tray on phones and tablets (below 1000 px the tray sits under every dish: 5,100 px down on a phone) | reached only by scrolling to the end or the top-bar link | a deep-green bar fixed to the bottom of the screen shows "2 dishes · ৳ 180 · Review tray" and jumps to the tray; hidden at 1000 px and wider, where the tray sits beside the dishes |
| Tables on phones (My reservations, order history, staff menu items, reservations, stock) | scrolled sideways inside a box (up to 652 px wide in a 341 px space) | each row is a card with the column names as labels (copied from the header row by `labelTableCells`); 341/341 px |
| Top bar height, phone (student pages / staff portal) | 164 / 212 px | 147 / 191 px |
| Top bar height, tablet (staff portal) | 164 px | 147 px |
| Controls under 44 px tall | brand link 38 px, skip link 41 px, tray quantity 40 px | none |
| Text under 12 px | brand subtitle 11.5 px | none |
| Desktop | content capped at 1180 px and centred; 3 dish columns with the tray beside them | unchanged |

## Re-check after the "linen and basil" palette (2026-10-24)

The palette follows how acclaimed restaurant sites use colour (mostly warm neutral space, one
strong brand colour saved for actions, serif display type). Headings now use the self-hosted
serif Cormorant Garamond. All 8 pages measured in the browser at 1280×800 and 375×812, every
visible piece of text against the background actually behind it.

| Check | Result |
|---|---|
| Contrast, measured on the rendered pages | 0 failing text elements on any page at either width; lowest ratio 5.44:1 (terracotta prices on white), body text 15.2:1 on linen and 16.8:1 on white |
| Contrast, colour pairs | `build/contrast_check` (ctest `contrast_check`): 21/21 pairs pass |
| Controls | Fields and selects have a 3.5:1 edge (`#8A8174`); buttons and tabs a 7.2:1 green edge; focus ring 7.2:1 on linen |
| Colour notation | `build/check_hex_colors` (ctest `hex_colors_only`): 24 files in `web/`, 0 non-hex colours |
| Reflow | No horizontal scroll on any page at 375 px |
| Browser surfaces | Text selection uses pale basil (`#E3EDE6`, text 14.0:1 on it); the caret is basil green |

### Refinement pass (same day)

Generic touches removed after a review against the frontend-design guidance: labels are in
sentence case (no `text-transform: uppercase` left anywhere), the small label above the landing
headline is gone, phrases joined by middle dots are plain sentences, the arrow is gone from
"Review tray", and basil green is the only accent (prices are bold charcoal). Re-measured all 8
pages at 1280 and 375 px: 0 failing text elements, lowest ratio 6.35:1, 0 uppercase elements, no
horizontal scroll, no control under 44 px. `build/contrast_check`: 22/22 pairs.
