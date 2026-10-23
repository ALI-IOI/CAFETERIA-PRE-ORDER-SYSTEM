# WCAG contrast check for the eggshell palette (one theme, no dark mode).
# Usage: python tools/contrast_check.py   -> exit code 1 if any pair fails.
import sys

def lum(h):
    h = h.lstrip('#')
    r, g, b = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    f = lambda c: c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    return 0.2126 * f(r) + 0.7152 * f(g) + 0.0722 * f(b)

def ratio(a, b):
    x, y = sorted([lum(a), lum(b)], reverse=True)
    return (x + 0.05) / (y + 0.05)

TEXT, UI = 4.5, 3.0
PAIRS = [
    # eggshell theme (one theme for everyone; spec 2026-10-24 Part 3)
    ("text on eggshell", "#16191C", "#EFE9D9", TEXT),
    ("text on cream", "#16191C", "#FAF7EF", TEXT),
    ("text on white top bar", "#16191C", "#FFFFFF", TEXT),
    ("text on well", "#16191C", "#E6DFCC", TEXT),
    ("secondary text on eggshell", "#4A463D", "#EFE9D9", TEXT),
    ("secondary text on cream", "#4A463D", "#FAF7EF", TEXT),
    ("secondary text on white", "#4A463D", "#FFFFFF", TEXT),
    ("white on deep green (bar, buttons)", "#FFFFFF", "#127A4B", TEXT),
    ("green text on eggshell", "#0F5C3A", "#EFE9D9", TEXT),
    ("green text on cream", "#0F5C3A", "#FAF7EF", TEXT),
    ("green text on white", "#0F5C3A", "#FFFFFF", TEXT),
    ("danger text on eggshell", "#A3241C", "#EFE9D9", TEXT),
    ("danger text on cream", "#A3241C", "#FAF7EF", TEXT),
    ("danger text on white", "#A3241C", "#FFFFFF", TEXT),
    ("danger text on danger tint", "#A3241C", "#F6DED8", TEXT),
    ("green edge on eggshell (UI)", "#127A4B", "#EFE9D9", UI),
    ("green edge on cream (UI)", "#127A4B", "#FAF7EF", UI),
    ("green edge on white (UI)", "#127A4B", "#FFFFFF", UI),
    ("focus ring on eggshell (UI)", "#127A4B", "#EFE9D9", UI),
    # Light stainless #C8CDD2 lines are decoration only (about 1.5:1), never a control's edge.
]

failed = 0
for name, fg, bg, need in PAIRS:
    r = ratio(fg, bg)
    ok = r >= need
    failed += not ok
    print(("PASS" if ok else "FAIL") + "  %5.2f:1  (need %.1f)  %s" % (r, need, name))
print("%d pairs, %d failed" % (len(PAIRS), failed))
sys.exit(1 if failed else 0)
