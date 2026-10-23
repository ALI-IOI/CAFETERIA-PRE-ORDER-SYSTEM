// WCAG 2.2 contrast check for the "linen and basil" palette (web/css/app.css).
// Every text/background and control/background pair the pages use is listed
// here with the ratio it must reach: 4.5:1 for text, 3:1 for controls and lines
// a person must see. Exit code 1 if any pair fails (run by ctest as contrast_check).
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
    {"text on linen page", "#1F1D1A", "#F7F3EC", TEXT},
    {"text on white surface", "#1F1D1A", "#FFFFFF", TEXT},
    {"text on well", "#1F1D1A", "#EFE9DE", TEXT},
    {"text on pale basil (selection, Ready chip)", "#1F1D1A", "#E3EDE6", TEXT},
    {"secondary text on linen", "#5E5850", "#F7F3EC", TEXT},
    {"secondary text on white", "#5E5850", "#FFFFFF", TEXT},
    {"secondary text on well", "#5E5850", "#EFE9DE", TEXT},
    {"white on basil (bar, buttons, badge)", "#FFFFFF", "#1E5B3E", TEXT},
    {"white on deep basil (button hover)", "#FFFFFF", "#143F2B", TEXT},
    {"green text on linen", "#1E5B3E", "#F7F3EC", TEXT},
    {"green text on white", "#1E5B3E", "#FFFFFF", TEXT},
    {"green text on pale basil", "#1E5B3E", "#E3EDE6", TEXT},
    {"green text on well (button hover)", "#1E5B3E", "#EFE9DE", TEXT},
    {"terracotta price on white", "#A4532F", "#FFFFFF", TEXT},
    {"terracotta price on linen", "#A4532F", "#F7F3EC", TEXT},
    {"danger text on linen", "#A3241C", "#F7F3EC", TEXT},
    {"danger text on white", "#A3241C", "#FFFFFF", TEXT},
    {"danger text on danger tint", "#A3241C", "#F6DED8", TEXT},
    {"demo notice text (online demo only)", "#1F1D1A", "#FFF8D6", TEXT},
    {"field edge on white (UI)", "#8A8174", "#FFFFFF", UI},
    {"field edge on linen (UI)", "#8A8174", "#F7F3EC", UI},
    {"green button edge on linen (UI)", "#1E5B3E", "#F7F3EC", UI},
    {"focus ring on linen (UI)", "#1E5B3E", "#F7F3EC", UI},
    {"focus ring on white (UI)", "#1E5B3E", "#FFFFFF", UI},
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
