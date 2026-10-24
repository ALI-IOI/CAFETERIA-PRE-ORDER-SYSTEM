// Static rules every page and the stylesheet must keep (ctest: page_rules).
// Pages: a language, a viewport, one h1, a skip link to #main, no inline styles
// or inline scripts, only /css/app.css, a label for every field, no arrows or
// decorative dots. Stylesheet: the type and spacing scales exist and every size
// uses the type scale, reduced motion is handled, no shadows, square corners,
// no capitals-only text, no transition or animation longer than 300 ms.
//
//   check_pages <web folder>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_problems = 0;

static void fail(const std::string& file, const std::string& rule) {
    std::printf("FAIL %s: %s\n", file.c_str(), rule.c_str());
    g_problems++;
}

static std::string readAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

static int countOf(const std::string& s, const std::regex& re) {
    return (int)std::distance(std::sregex_iterator(s.begin(), s.end(), re), std::sregex_iterator());
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// ---- pages -----------------------------------------------------------------

static void htmlLanguage(const std::string& f, const std::string& h) {
    if (h.find("<html lang=\"en\"") == std::string::npos) fail(f, "<html lang=\"en\"> missing");
}

static void htmlViewport(const std::string& f, const std::string& h) {
    if (h.find("<meta name=\"viewport\"") == std::string::npos) fail(f, "viewport meta missing");
}

static void htmlOneH1(const std::string& f, const std::string& h) {
    int n = countOf(h, std::regex(R"(<h1[\s>])"));
    if (n != 1) fail(f, "needs exactly one <h1>, has " + std::to_string(n));
}

static void htmlSkipLinkAndMain(const std::string& f, const std::string& h) {
    if (h.find("href=\"#main\"") == std::string::npos) fail(f, "skip link to #main missing");
    if (h.find("id=\"main\"") == std::string::npos) fail(f, "no element with id=\"main\"");
}

static void htmlNoInlineStyle(const std::string& f, const std::string& h) {
    if (std::regex_search(h, std::regex(R"(\sstyle\s*=)"))) fail(f, "inline style= attribute");
    if (std::regex_search(h, std::regex(R"(<style[\s>])"))) fail(f, "inline <style> block");
}

static void htmlScriptsHaveSrc(const std::string& f, const std::string& h) {
    const std::regex tag(R"(<script\b[^>]*>)");
    for (auto it = std::sregex_iterator(h.begin(), h.end(), tag); it != std::sregex_iterator(); ++it)
        if (it->str().find("src=") == std::string::npos) fail(f, "inline <script> without src");
}

static void htmlOnlyAppCss(const std::string& f, const std::string& h) {
    const std::regex link(R"(<link\b[^>]*rel="stylesheet"[^>]*>)");
    int n = 0;
    for (auto it = std::sregex_iterator(h.begin(), h.end(), link); it != std::sregex_iterator(); ++it) {
        n++;
        if (it->str().find("/css/app.css") == std::string::npos) fail(f, "stylesheet other than /css/app.css");
    }
    if (n != 1) fail(f, "needs exactly one stylesheet, has " + std::to_string(n));
}

static void htmlFieldsLabelled(const std::string& f, const std::string& h) {
    const std::regex field(R"(<(input|select|textarea)\b[^>]*>)");
    const std::regex idAttr(R"re(\sid="([^"]+)")re");
    for (auto it = std::sregex_iterator(h.begin(), h.end(), field); it != std::sregex_iterator(); ++it) {
        const std::string t = it->str();
        if (t.find("type=\"hidden\"") != std::string::npos || t.find("type=\"submit\"") != std::string::npos) continue;
        if (t.find("aria-label=") != std::string::npos || t.find("aria-labelledby=") != std::string::npos) continue;
        std::smatch m;
        if (!std::regex_search(t, m, idAttr)) {
            fail(f, "field without id or aria-label: " + t.substr(0, 60));
            continue;
        }
        if (h.find("for=\"" + m[1].str() + "\"") == std::string::npos) fail(f, "no <label for=\"" + m[1].str() + "\">");
    }
}

static void htmlNoArrowsOrDots(const std::string& f, const std::string& h) {
    for (const char* bad : {"\xE2\x86\x92", "\xC2\xBB", "\xC2\xB7", "&rarr;", "&raquo;", "&middot;"})
        if (h.find(bad) != std::string::npos) fail(f, std::string("arrow or decorative dot in the page: ") + bad);
}

// ---- stylesheet ------------------------------------------------------------

static std::string stripComments(const std::string& css) {
    std::string out;
    for (size_t i = 0; i < css.size(); i++) {
        if (css.compare(i, 2, "/*") == 0) {
            size_t end = css.find("*/", i + 2);
            if (end == std::string::npos) break;
            i = end + 1;
        } else {
            out += css[i];
        }
    }
    return out;
}

struct Decl {
    std::string selector;  // innermost rule selector ("" at top level)
    std::string prop;
    std::string value;
};

// Declarations with the selector of the rule they sit in (at-rules are passed through).
static std::vector<Decl> declarations(const std::string& css) {
    std::vector<Decl> out;
    std::vector<std::string> stack;
    std::string buf;
    for (char c : css) {
        if (c == '{') {
            stack.push_back(trim(buf));
            buf.clear();
        } else if (c == '}' || c == ';') {
            std::string d = trim(buf);
            size_t colon = d.find(':');
            std::string sel;
            for (auto s = stack.rbegin(); s != stack.rend(); ++s)
                if (!s->empty() && (*s)[0] != '@') { sel = *s; break; }
            if (colon != std::string::npos && !stack.empty() && !sel.empty())
                out.push_back({sel, trim(d.substr(0, colon)), trim(d.substr(colon + 1))});
            buf.clear();
            if (c == '}' && !stack.empty()) stack.pop_back();
        } else {
            buf += c;
        }
    }
    return out;
}

static void cssScales(const std::string& f, const std::string& css) {
    for (const char* t : {"--step--1:", "--step-0:", "--step-1:", "--step-2:", "--step-3:", "--step-4:"})
        if (css.find(t) == std::string::npos) fail(f, std::string("type scale token missing: ") + t);
    for (int i = 1; i <= 8; i++) {
        std::string t = "--space-" + std::to_string(i) + ":";
        if (css.find(t) == std::string::npos) fail(f, "spacing token missing: " + t);
    }
}

// With reduced motion, every change is instant: the block must switch off every
// animation and transition, not only the movement.
static void cssReducedMotion(const std::string& f, const std::string& css) {
    size_t at = css.find("@media (prefers-reduced-motion: reduce)");
    if (at == std::string::npos) {
        fail(f, "no @media (prefers-reduced-motion: reduce) block");
        return;
    }
    size_t open = css.find('{', at), depth = 0, end = open;
    for (; end < css.size(); end++) {
        if (css[end] == '{') depth++;
        else if (css[end] == '}' && --depth == 0) break;
    }
    const std::string block = css.substr(open, end - open);
    if (block.find("animation: none !important") == std::string::npos)
        fail(f, "reduced motion does not set animation: none !important");
    if (block.find("transition: none !important") == std::string::npos)
        fail(f, "reduced motion does not set transition: none !important");
}

static void cssDeclarationRules(const std::string& f, const std::vector<Decl>& decls) {
    const std::regex duration(R"((\d*\.?\d+)(ms|s)\b)");
    for (const Decl& d : decls) {
        const std::string where = " (" + d.selector.substr(0, 50) + ")";
        if (d.prop == "box-shadow") fail(f, "box-shadow" + where);
        if (d.prop == "border-radius" && d.value != "var(--radius)" && d.value != "0")
            fail(f, "border-radius " + d.value + where);
        if (d.prop == "text-transform" && d.value.find("uppercase") != std::string::npos)
            fail(f, "text-transform: uppercase" + where);
        if (d.selector != ":root" && d.prop == "font-size" && d.value.find("var(--step-") == std::string::npos)
            fail(f, "font-size not on the type scale: " + d.value + where);
        if (d.selector != ":root" && d.prop == "font" && d.value != "inherit" &&
            d.value.find("var(--step-") == std::string::npos)
            fail(f, "font shorthand size not on the type scale: " + d.value + where);
        if (d.prop == "transition" || d.prop == "transition-duration" || d.prop == "animation" ||
            d.prop == "animation-duration" || d.prop.rfind("--dur", 0) == 0) {
            for (auto it = std::sregex_iterator(d.value.begin(), d.value.end(), duration); it != std::sregex_iterator();
                 ++it) {
                double v = std::atof((*it)[1].str().c_str()) * ((*it)[2].str() == "s" ? 1000.0 : 1.0);
                if (v > 300.0) fail(f, d.prop + " longer than 300ms: " + d.value + where);
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2 || !fs::is_directory(argv[1])) {
        std::fprintf(stderr, "Usage: check_pages <web folder>\n");
        return 2;
    }
    const fs::path root = argv[1];
    int files = 0;
    for (const auto& e : fs::recursive_directory_iterator(root)) {
        if (!e.is_regular_file() || e.path().extension() != ".html") continue;
        files++;
        const std::string f = fs::relative(e.path(), root).generic_string();
        const std::string h = readAll(e.path());
        htmlLanguage(f, h);
        htmlViewport(f, h);
        htmlOneH1(f, h);
        htmlSkipLinkAndMain(f, h);
        htmlNoInlineStyle(f, h);
        htmlScriptsHaveSrc(f, h);
        htmlOnlyAppCss(f, h);
        htmlFieldsLabelled(f, h);
        htmlNoArrowsOrDots(f, h);
    }
    const fs::path cssPath = root / "css" / "app.css";
    if (fs::is_regular_file(cssPath)) {
        files++;
        const std::string css = stripComments(readAll(cssPath));
        cssScales("css/app.css", css);
        cssReducedMotion("css/app.css", css);
        cssDeclarationRules("css/app.css", declarations(css));
    } else {
        fail("css/app.css", "missing");
    }
    std::printf("%d files, %d problems\n", files, g_problems);
    return g_problems ? 1 : 0;
}
