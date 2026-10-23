# WCAG contrast check for the green-and-stainless palette (light and dark).
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
    # light theme
    ("light: text on concrete", "#16191C", "#F1F2F3", TEXT),
    ("light: text on card", "#16191C", "#FFFFFF", TEXT),
    ("light: text on stainless", "#16191C", "#C8CDD2", TEXT),
    ("light: secondary text on concrete", "#3A4046", "#F1F2F3", TEXT),
    ("light: secondary text on stainless", "#3A4046", "#C8CDD2", TEXT),
    ("light: white on machine green button", "#FFFFFF", "#1F6B45", TEXT),
    ("light: white on graphite button", "#FFFFFF", "#16191C", TEXT),
    ("light: machine green text on concrete", "#1F6B45", "#F1F2F3", TEXT),
    ("light: machine green text on card", "#1F6B45", "#FFFFFF", TEXT),
    ("light: deep green text on stainless", "#0F5C3A", "#C8CDD2", TEXT),
    ("light: topbar text on graphite", "#F1F2F3", "#16191C", TEXT),
    ("light: danger text on card", "#A3241C", "#FFFFFF", TEXT),
    ("light: green accent rule on stainless (UI)", "#1F6B45", "#C8CDD2", UI),
    ("light: steel border on concrete (UI)", "#6B737B", "#F1F2F3", UI),
    ("light: focus ring on concrete (UI)", "#1F6B45", "#F1F2F3", UI),
    ("light: focus ring on graphite bar (UI)", "#7FD1A4", "#16191C", UI),
    # dark theme
    ("dark: text on page", "#ECEEF0", "#121416", TEXT),
    ("dark: text on panel", "#ECEEF0", "#1D2125", TEXT),
    ("dark: secondary text on panel", "#B6BCC2", "#1D2125", TEXT),
    ("dark: green text on panel", "#7FD1A4", "#1D2125", TEXT),
    ("dark: dark text on green button", "#0B1F15", "#7FD1A4", TEXT),
    ("dark: text on hero steel", "#ECEEF0", "#2A3036", TEXT),
    ("dark: danger text on panel", "#FF9B92", "#1D2125", TEXT),
    ("dark: border on panel (UI)", "#69727B", "#1D2125", UI),
]

failed = 0
for name, fg, bg, need in PAIRS:
    r = ratio(fg, bg)
    ok = r >= need
    failed += not ok
    print(("PASS" if ok else "FAIL") + "  %5.2f:1  (need %.1f)  %s" % (r, need, name))
print("%d pairs, %d failed" % (len(PAIRS), failed))
sys.exit(1 if failed else 0)
