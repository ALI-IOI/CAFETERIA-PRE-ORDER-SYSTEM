// Runs every registered test. Optional argument: only run tests whose name
// contains that text. Exit code = number of failed tests.
#include "testing.h"
#include <cstdio>
#include <cstring>

static TestCase* g_head = nullptr;
static TestCase* g_tail = nullptr;
static int g_currentFailures = 0;

int registerTest(TestCase* t) {
    t->next = nullptr;
    if (!g_head) g_head = t;
    else g_tail->next = t;
    g_tail = t;
    return 0;
}

void reportFailure(const char* file, int line, const char* expr, const std::string& detail) {
    g_currentFailures++;
    std::printf("    %s:%d: CHECK failed: %s", file, line, expr);
    if (!detail.empty()) std::printf("  [%s]", detail.c_str());
    std::printf("\n");
}

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failed = 0;
    for (TestCase* t = g_head; t; t = t->next) {
        if (filter && !std::strstr(t->name, filter)) continue;
        g_currentFailures = 0;
        t->fn();
        run++;
        if (g_currentFailures) {
            failed++;
            std::printf("FAIL %s\n", t->name);
        } else {
            std::printf("PASS %s\n", t->name);
        }
    }
    std::printf("\n%d tests, %d passed, %d failed\n", run, run - failed, failed);
    return failed;
}
