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
