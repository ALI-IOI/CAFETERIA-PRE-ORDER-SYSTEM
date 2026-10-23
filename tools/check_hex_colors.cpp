// Fails if any colour in the web pages is written other than as hex (#RGB,
// #RRGGBB or #RRGGBBAA): no rgb()/rgba()/hsl()/hsla()/hwb()/lab()/lch()/oklab()/
// oklch()/color-mix()/color(), and no colour keywords (white, black,
// transparent, currentColor, ...). Scans .css, .html, .js and .svg files under
// the folders given on the command line (ctest: web/ and pages-demo/).
//
//   check_hex_colors <folder>...
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

namespace fs = std::filesystem;

// The CSS colour keywords (CSS Color 4 named colours plus the special ones).
static const char* const KEYWORDS =
    "transparent|currentcolor|aliceblue|antiquewhite|aqua|aquamarine|azure|beige|bisque|black|blanchedalmond|blue|"
    "blueviolet|brown|burlywood|cadetblue|chartreuse|chocolate|coral|cornflowerblue|cornsilk|crimson|cyan|darkblue|"
    "darkcyan|darkgoldenrod|darkgray|darkgreen|darkgrey|darkkhaki|darkmagenta|darkolivegreen|darkorange|darkorchid|"
    "darkred|darksalmon|darkseagreen|darkslateblue|darkslategray|darkslategrey|darkturquoise|darkviolet|deeppink|"
    "deepskyblue|dimgray|dimgrey|dodgerblue|firebrick|floralwhite|forestgreen|fuchsia|gainsboro|ghostwhite|gold|"
    "goldenrod|gray|green|greenyellow|grey|honeydew|hotpink|indianred|indigo|ivory|khaki|lavender|lavenderblush|"
    "lawngreen|lemonchiffon|lightblue|lightcoral|lightcyan|lightgoldenrodyellow|lightgray|lightgreen|lightgrey|"
    "lightpink|lightsalmon|lightseagreen|lightskyblue|lightslategray|lightslategrey|lightsteelblue|lightyellow|lime|"
    "limegreen|linen|magenta|maroon|mediumaquamarine|mediumblue|mediumorchid|mediumpurple|mediumseagreen|"
    "mediumslateblue|mediumspringgreen|mediumturquoise|mediumvioletred|midnightblue|mintcream|mistyrose|moccasin|"
    "navajowhite|navy|oldlace|olive|olivedrab|orange|orangered|orchid|palegoldenrod|palegreen|paleturquoise|"
    "palevioletred|papayawhip|peachpuff|peru|pink|plum|powderblue|purple|rebeccapurple|red|rosybrown|royalblue|"
    "saddlebrown|salmon|sandybrown|seagreen|seashell|sienna|silver|skyblue|slateblue|slategray|slategrey|snow|"
    "springgreen|steelblue|tan|teal|thistle|tomato|turquoise|violet|wheat|white|whitesmoke|yellow|yellowgreen";

static bool endsWith(const std::string& s, const char* tail) {
    std::string t(tail);
    return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: check_hex_colors <folder>...\n");
        return 2;
    }
    // Functional colour notations, anywhere.
    const std::regex functional(R"((^|[^a-zA-Z0-9_-])(rgba?|hsla?|hwb|lab|lch|oklab|oklch|color-mix|color)\s*\()",
                                std::regex::icase);
    // A keyword as a colour value: after a colour property in CSS or a style
    // attribute, or as an SVG/HTML colour attribute.
    const std::string kw = std::string("(") + KEYWORDS + ")";
    const std::regex cssValue("(^|[;{\\s\"'])(color|background|background-color|border|border-(top|right|bottom|left)(-color)?|"
                              "border-color|outline|outline-color|fill|stroke|stop-color|flood-color|lighting-color|caret-color|"
                              "accent-color|column-rule|column-rule-color|text-decoration|text-decoration-color|"
                              "text-emphasis-color|box-shadow|text-shadow|--[a-z0-9-]+)\\s*:[^;{}]*?(^|[^a-z0-9#-])" +
                                  kw + "([^a-z0-9-]|$)",
                              std::regex::icase);
    const std::regex attrValue("(fill|stroke|stop-color|flood-color|color)\\s*=\\s*[\"']\\s*" + kw + "\\s*[\"']",
                               std::regex::icase);

    int files = 0, problems = 0;
    for (int a = 1; a < argc; a++) {
        if (!fs::is_directory(argv[a])) {
            std::fprintf(stderr, "not a folder: %s\n", argv[a]);
            return 2;
        }
        for (const auto& entry : fs::recursive_directory_iterator(argv[a])) {
            if (!entry.is_regular_file()) continue;
            const std::string path = entry.path().generic_string();
            if (!(endsWith(path, ".css") || endsWith(path, ".html") || endsWith(path, ".js") || endsWith(path, ".svg")))
                continue;
            files++;
            std::ifstream in(entry.path());
            std::string line;
            int lineNo = 0;
            bool inComment = false;
            while (std::getline(in, line)) {
                lineNo++;
                // Leave out CSS/JS block comments and line comments: they describe colours in words.
                std::string code;
                for (size_t i = 0; i < line.size(); i++) {
                    if (inComment) {
                        if (line.compare(i, 2, "*/") == 0) inComment = false, i++;
                    } else if (line.compare(i, 2, "/*") == 0) {
                        inComment = true, i++;
                    } else if (line.compare(i, 2, "//") == 0 && (i == 0 || line[i - 1] != ':')) {
                        break;
                    } else {
                        code += line[i];
                    }
                }
                bool js = endsWith(path, ".js");
                bool bad = std::regex_search(code, functional) ||
                           (!js && (std::regex_search(code, cssValue) || std::regex_search(code, attrValue)));
                if (bad) {
                    std::printf("%s:%d: non-hex colour: %s\n", path.c_str(), lineNo, line.c_str());
                    problems++;
                }
            }
        }
    }
    std::printf("%d files scanned, %d non-hex colour(s)\n", files, problems);
    return problems ? 1 : 0;
}
