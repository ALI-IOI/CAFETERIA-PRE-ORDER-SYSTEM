// WCAG 2.2 contrast check for the "teal and navy" palette (web/css/app.css).
// Every text/background and control/background pair the pages use is listed
// here with the ratio it must reach: 4.5:1 for text, 3:1 for controls and lines
// a person must see. Exit code 1 if any pair fails (run by ctest as contrast_check).
//
// Two pairs are never used, on purpose, because they fail:
//   secondary grey #5B5F63 on the card teal #B9DADB (4.33:1) - only navy goes there;
//   orange #E87722 as text, a line or a ring on white (2.96:1) - orange is only ever
//   a filled shape with navy text on it.
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
    {"navy text on white page", "#002855", "#FFFFFF", TEXT},
    {"navy text on well", "#002855", "#EEF5F5", TEXT},
    {"navy text on pale teal (selection, ready row)", "#002855", "#DCEDEE", TEXT},
    {"navy text on card teal (hero field, lead surfaces)", "#002855", "#B9DADB", TEXT},
    {"navy text on orange (Ready chip, tray count)", "#002855", "#E87722", TEXT},
    {"secondary text on white", "#5B5F63", "#FFFFFF", TEXT},
    {"secondary text on well", "#5B5F63", "#EEF5F5", TEXT},
    {"secondary text on pale teal (ready row)", "#5B5F63", "#DCEDEE", TEXT},
    {"white on teal (header, buttons, tray bar)", "#FFFFFF", "#007377", TEXT},
    {"white on deep teal (button hover)", "#FFFFFF", "#005B5E", TEXT},
    {"white on navy (announcement bar)", "#FFFFFF", "#002855", TEXT},
    {"teal text and links on white", "#007377", "#FFFFFF", TEXT},
    {"teal text on well (button hover)", "#007377", "#EEF5F5", TEXT},
    {"teal text on pale teal (stack top)", "#007377", "#DCEDEE", TEXT},
    {"danger text on white", "#A3241C", "#FFFFFF", TEXT},
    {"danger text on well", "#A3241C", "#EEF5F5", TEXT},
    {"danger text on danger tint", "#A3241C", "#F6DED8", TEXT},
    {"navy text on danger tint (closed banner)", "#002855", "#F6DED8", TEXT},
    {"secondary text on danger tint", "#5B5F63", "#F6DED8", TEXT},
    {"field edge on white (UI)", "#5B5F63", "#FFFFFF", UI},
    {"field edge on well (UI)", "#5B5F63", "#EEF5F5", UI},
    {"dotted leader on white (UI)", "#5B5F63", "#FFFFFF", UI},
    {"teal button edge on white (UI)", "#007377", "#FFFFFF", UI},
    {"teal edge on card teal (UI)", "#007377", "#B9DADB", UI},
    {"navy outline of the orange chip on white (UI)", "#002855", "#FFFFFF", UI},
    {"focus ring on white (UI)", "#002855", "#FFFFFF", UI},
    {"focus ring on well (UI)", "#002855", "#EEF5F5", UI},
    {"focus ring on card teal (UI)", "#002855", "#B9DADB", UI},
    {"header and tray bar focus ring: white on teal (UI)", "#FFFFFF", "#007377", UI},
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
