// Regression test for a crash when threads that used cpp-httplib exit.
//
// cpp-httplib keeps some objects (regexes, lookup sets) in thread_local
// storage. In this static MinGW build their destructors can run on memory that
// is already freed when a thread exits, crashing the process at random (seen
// as exit code 0xC0000374 in the E2E test). The vendored httplib.h is patched
// to use `static const` for those read-only objects (third_party/httplib/PATCHES.md).
//
// The test starts a server in-process and runs many short-lived client
// threads, each of which makes a request (parsing the status line) and exits.
// Exit code 0 = no crash.
#include "httplib.h"
#include <atomic>
#include <cstdio>
#include <thread>

int main() {
    httplib::Server svr;
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res) { res.set_content("pong", "text/plain"); });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread server([&]() { svr.listen_after_bind(); });
    svr.wait_until_ready();

    std::atomic<int> ok(0);
    const int rounds = 40, perRound = 16;
    for (int r = 0; r < rounds; r++) {
        std::thread workers[perRound];
        for (auto& w : workers) {
            w = std::thread([&]() {
                httplib::Client c("127.0.0.1", port);
                auto res = c.Get("/ping");
                if (res && res->status == 200) ok++;
            });  // the thread exits here: its thread_local destructors run
        }
        for (auto& w : workers) w.join();
    }
    svr.stop();
    server.join();
    std::printf("%d of %d requests ok\n", ok.load(), rounds * perRound);
    return ok.load() == rounds * perRound ? 0 : 1;
}
