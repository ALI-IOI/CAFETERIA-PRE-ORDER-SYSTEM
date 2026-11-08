// WCAG 2.2 contrast check for the "chili and rice" palette (web/css/app.css).
// Every text/background and control/background pair the pages use is listed
// here with the ratio it must reach: 4.5:1 for text, 3:1 for controls and lines
// a person must see. Exit code 1 if any pair fails (run by ctest as contrast_check).
//
// Pairs never used, on purpose, because they fail:
//   turmeric #F2B531 as text, a line or a ring on cream (1.7:1) - turmeric is only a
//   filled shape with tamarind text on it;
//   mint #9ED7C3 as text or a line on cream - mint is only a filled panel or stamp.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Pair {
    const char* name;
    const char* fg;
    const char* bg;
    double need;
};

static const double TEXT = 4.5, UI = 3.0;

static const Pair PAIRS[] = {
    {"tamarind text on cream page", "#2A1410", "#FBF1E4", TEXT},
    {"tamarind text on panel", "#2A1410", "#FFFAF3", TEXT},
    {"tamarind text on well", "#2A1410", "#F6E6D2", TEXT},
    {"tamarind text on pale saffron (selection, ready row)", "#2A1410", "#FBE3C8", TEXT},
    {"tamarind text on mint (quiet stamps)", "#2A1410", "#9ED7C3", TEXT},
    {"tamarind text on turmeric (Ready chip, tray count, stamps)", "#2A1410", "#F2B531", TEXT},
    {"secondary text on cream", "#6A4237", "#FBF1E4", TEXT},
    {"secondary text on panel", "#6A4237", "#FFFAF3", TEXT},
    {"secondary text on well", "#6A4237", "#F6E6D2", TEXT},
    {"secondary text on pale saffron (ready row)", "#6A4237", "#FBE3C8", TEXT},
    {"cream on chili (header, home field, buttons, tray bar)", "#FBF1E4", "#B42318", TEXT},
    {"cream on deep chili (button hover)", "#FBF1E4", "#8A1810", TEXT},
    {"cream on tamarind (announcement bar, ink stamp)", "#FBF1E4", "#2A1410", TEXT},
    {"cream on curry leaf (ready stamps)", "#FBF1E4", "#2F6B3C", TEXT},
    {"chili text and links on cream", "#B42318", "#FBF1E4", TEXT},
    {"chili text on panel (stamps, outline buttons)", "#B42318", "#FFFAF3", TEXT},
    {"chili text on well (button hover)", "#B42318", "#F6E6D2", TEXT},
    {"chili text on pale saffron (stack top, cream button hover)", "#B42318", "#FBE3C8", TEXT},
    {"curry leaf text on cream (success messages)", "#2F6B3C", "#FBF1E4", TEXT},
    {"danger text on cream", "#9B1C1C", "#FBF1E4", TEXT},
    {"danger text on panel", "#9B1C1C", "#FFFAF3", TEXT},
    {"danger text on danger tint", "#9B1C1C", "#F8DCD5", TEXT},
    {"tamarind text on danger tint (closed banner)", "#2A1410", "#F8DCD5", TEXT},
    {"secondary text on danger tint", "#6A4237", "#F8DCD5", TEXT},
    {"field edge on cream (UI)", "#8C6A5C", "#FBF1E4", UI},
    {"field edge on panel (UI)", "#8C6A5C", "#FFFAF3", UI},
    {"dotted leader on panel (UI)", "#8C6A5C", "#FFFAF3", UI},
    {"chili button edge and stamp ring on cream (UI)", "#B42318", "#FBF1E4", UI},
    {"cream stamp ring and ghost button edge on chili (UI)", "#FBF1E4", "#B42318", UI},
    {"curry-leaf stamp on cream (UI)", "#2F6B3C", "#FBF1E4", UI},
    {"tamarind outline of the turmeric chip on cream (UI)", "#2A1410", "#FBF1E4", UI},
    {"focus ring on cream (UI)", "#2A1410", "#FBF1E4", UI},
    {"focus ring on panel (UI)", "#2A1410", "#FFFAF3", UI},
    {"focus ring on mint (UI)", "#2A1410", "#9ED7C3", UI},
    {"header, home field and tray bar focus ring: cream on chili (UI)", "#FBF1E4", "#B42318", UI},
};

// "#RRGGBB" -> relative luminance (WCAG 2.2).
static double luminance(const char* hex) {
    double c[3];
    for (int i = 0; i < 3; i++) {
        char part[3] = {hex[1 + 2 * i], hex[2 + 2 * i], 0};
        double v = std::strtol(part, nullptr, 16) / 255.0;
        c[i] = v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    }
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
}

static double ratio(const char* a, const char* b) {
    double x = luminance(a), y = luminance(b);
    if (x < y) {
        double t = x;
        x = y;
        y = t;
    }
    return (x + 0.05) / (y + 0.05);
}

int main() {
    int n = (int)(sizeof PAIRS / sizeof PAIRS[0]), failed = 0;
    for (int i = 0; i < n; i++) {
        const Pair& p = PAIRS[i];
        if (std::strlen(p.fg) != 7 || std::strlen(p.bg) != 7) {
            std::printf("BAD   %s: colours must be #RRGGBB\n", p.name);
            failed++;
            continue;
        }
        double r = ratio(p.fg, p.bg);
        bool ok = r >= p.need;
        failed += ok ? 0 : 1;
        std::printf("%s  %5.2f:1  (need %.1f)  %s\n", ok ? "PASS" : "FAIL", r, p.need, p.name);
    }
    std::printf("%d pairs, %d failed\n", n, failed);
    return failed ? 1 : 0;
}
