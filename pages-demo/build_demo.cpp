// Builds the online demo for GitHub Pages from web/ (the real pages, unchanged in
// the repo): absolute paths get the Pages base path, and pages-demo/demo-api.js
// (an in-browser stand-in for the C++ server) loads before js/api.js.
//
//   build_demo <output dir> [base path, default /cafeteria-pre-order-system/]
//
// Run it from the repository root (it reads web/ and pages-demo/ from there).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string readAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

static void writeAll(const fs::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary);
    out << text;
}

static void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
}

static void replaceFirst(std::string& s, const std::string& from, const std::string& to) {
    size_t at = s.find(from);
    if (at != std::string::npos) s.replace(at, from.size(), to);
}

static bool endsWith(const std::string& s, const std::string& tail) {
    return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::fprintf(stderr, "Usage: build_demo <output dir> [base path, default /cafeteria-pre-order-system/]\n");
        return 2;
    }
    const fs::path out = fs::absolute(argv[1]);
    const std::string base = argc == 3 ? argv[2] : "/cafeteria-pre-order-system/";
    if (base.empty() || base.front() != '/' || base.back() != '/') {
        std::fprintf(stderr, "base path must start and end with /\n");
        return 2;
    }
    if (!fs::is_directory("web") || !fs::is_regular_file("pages-demo/demo-api.js")) {
        std::fprintf(stderr, "run this from the repository root (web/ and pages-demo/ not found)\n");
        return 2;
    }

    fs::remove_all(out);
    fs::copy("web", out, fs::copy_options::recursive);
    fs::copy_file("pages-demo/demo-api.js", out / "demo-api.js");
    writeAll(out / ".nojekyll", "");

    // "/css/app.css", '/menu.html#tray', url("/img/x.svg") ... -> base-prefixed. A bare
    // "/" is text in some scripts (dates, "3/100"), so only its two link forms change.
    const std::regex absPath(
        R"((["'`(])/((?:css|js|img|fonts|staff|images|api)/[^"'`)\s]*|[a-z-]+\.html(?:#[^"'`)\s]*)?)(?=["'`)]))");
    const std::string homeLinks[][2] = {{"href=\"/\"", "href=\"" + base + "\""},
                                        {"wireLogout(\"logout\", \"/\")", "wireLogout(\"logout\", \"" + base + "\")"}};
    const std::string noteCss =
        "\n/* ---- online demo notice (GitHub Pages build only) ---- */\n"
        ".demo-note { background: #FFF8D6; color: var(--ink); border-bottom: 1px solid var(--line); font-size: 0.85rem; "
        "padding: 0.5rem var(--space); }\n"
        ".demo-note p { max-width: 1180px; margin: 0 auto; }\n"
        ".demo-note .linkbtn { color: var(--accent-text); font-weight: 600; min-height: 44px; }\n";

    int changed = 0;
    std::vector<fs::path> pages;
    for (const auto& entry : fs::recursive_directory_iterator(out)) {
        if (!entry.is_regular_file()) continue;
        const fs::path& path = entry.path();
        const std::string name = path.filename().string();
        if (name == "demo-api.js" || !(endsWith(name, ".html") || endsWith(name, ".js") || endsWith(name, ".css"))) continue;
        const std::string text = readAll(path);
        std::string next = std::regex_replace(text, absPath, "$1" + base + "$2");
        for (const auto& link : homeLinks) replaceAll(next, link[0], link[1]);
        if (endsWith(name, ".html")) {
            replaceFirst(next, "<script src=\"" + base + "js/api.js\"></script>",
                         "<script src=\"" + base + "demo-api.js\"></script>\n  <script src=\"" + base + "js/api.js\"></script>");
            replaceFirst(next, "<title>", "<title>Demo \xC2\xB7 ");
            pages.push_back(path);
        }
        if (name == "app.css") next += noteCss;
        if (next != text) {
            writeAll(path, next);
            changed++;
        }
    }

    std::string missing;
    for (const auto& page : pages)
        if (readAll(page).find("demo-api.js") == std::string::npos) missing += " " + fs::relative(page, out).generic_string();
    if (!missing.empty()) {
        std::fprintf(stderr, "demo-api.js not loaded on:%s\n", missing.c_str());
        return 1;
    }
    std::printf("built %s (base %s): %d files rewritten, %zu pages\n", out.generic_string().c_str(), base.c_str(), changed,
                pages.size());
    return 0;
}
