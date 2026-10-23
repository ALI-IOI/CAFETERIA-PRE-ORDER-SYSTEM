// Cafeteria pre-order web server.
//
//   cafeteria_server [--port 8080] [--host 127.0.0.1] [--data data] [--config config]
//                    [--seed seed] [--web web] [--tick-seconds 60] [--behind-https]
//                    [--fake-clock YYYY-MM-DDTHH:MM]   (testing/demo only)
//
// Start-up (proposal section 8): load menu, users and reservations from files,
// rebuild the queues, empty the stack, start the minute timer and the server.
#include "auth/crypto.h"
#include "auth/user_store.h"
#include "httplib.h"
#include "server/app.h"
#include "server/routes.h"
#include "storage/file_util.h"
#include "storage/storage.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
#ifdef _WIN32
#include <process.h>
#include <windows.h>
#endif

static httplib::Server* g_server = nullptr;
static std::atomic<bool> g_stop(false);

#ifdef _WIN32
static BOOL WINAPI onCtrl(DWORD) {
    g_stop = true;
    if (g_server) g_server->stop();
    return TRUE;
}
#endif

// Lists every line marked [CONFIRM] so nobody forgets a placeholder.
static void printPlaceholders(const std::string* files, int n) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        std::ifstream in(files[i]);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("[CONFIRM") != std::string::npos) {
                if (count++ == 0) std::printf("Placeholders still to confirm:\n");
                std::printf("  %s: %s\n", files[i].c_str(), line.c_str());
            }
        }
    }
}

static bool parseFakeClock(const std::string& s, FakeClock& fc) {
    int y, mo, d, h, mi;
    if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) != 5) return false;
    fc.set(y, mo, d, h, mi, 0);
    return true;
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1", dataDir = "data", configDir = "config", seedDir = "seed", webDir = "web";
    std::string fakeClockArg;
    int port = 8080, tickSeconds = 60;
    bool behindHttps = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        bool hasValue = i + 1 < argc;
        if (a == "--port" && hasValue) port = std::atoi(argv[++i]);
        else if (a == "--host" && hasValue) host = argv[++i];
        else if (a == "--data" && hasValue) dataDir = argv[++i];
        else if (a == "--config" && hasValue) configDir = argv[++i];
        else if (a == "--seed" && hasValue) seedDir = argv[++i];
        else if (a == "--web" && hasValue) webDir = argv[++i];
        else if (a == "--tick-seconds" && hasValue) tickSeconds = std::atoi(argv[++i]);
        else if (a == "--fake-clock" && hasValue) fakeClockArg = argv[++i];
        else if (a == "--behind-https") behindHttps = true;
        else {
            std::fprintf(stderr, "Unknown or incomplete option: %s\n", a.c_str());
            return 2;
        }
    }

    AppConfig cfg;
    SmtpConfig smtp;
    std::string err;
    if (!loadAppConfig(configDir + "/app.cfg", cfg, err) || !loadSmtpConfig(configDir + "/smtp.cfg", smtp, err)) {
        std::fprintf(stderr, "Config error: %s\n", err.c_str());
        return 1;
    }

    static SystemClock systemClock;
    static FakeClock fakeClock;
    AppState app;
    app.clock = &systemClock;
    if (!fakeClockArg.empty()) {
        if (!parseFakeClock(fakeClockArg, fakeClock)) {
            std::fprintf(stderr, "--fake-clock expects YYYY-MM-DDTHH:MM\n");
            return 2;
        }
        app.clock = &fakeClock;
        app.fakeClock = &fakeClock;
        std::printf("WARNING: fake clock enabled (testing/demo only). POST /api/dev/advance moves time.\n");
    }
    app.dataDir = dataDir;
    app.secureCookies = behindHttps;

    if (!cryptoInit()) {
        std::fprintf(stderr, "libsodium failed to initialise\n");
        return 1;
    }
    cafInit(app.caf, app.clock, cfg.cookingCapacity);
    AuthConfig ac = authDefaults();
    ac.emailDomain = cfg.emailDomain;
    ac.sessionMinutes = cfg.sessionMinutes;
    ac.lockoutFailures = cfg.lockoutFailures;
    ac.lockoutMinutes = cfg.lockoutMinutes;
    ac.codeMinutes = cfg.codeMinutes;
    authInit(app.auth, app.clock, ac);

    if (!loadCafeteria(app.caf, dataDir, seedDir, err) || !loadUsers(app.auth.users, dataDir, err)) {
        std::fprintf(stderr, "Cannot load data: %s\n", err.c_str());
        return 1;
    }
    std::string uniPath = configDir + "/university_list.csv";
    if (fileExists(uniPath)) {
        if (!uniLoadCsv(app.auth.uni, uniPath, err)) {
            std::fprintf(stderr, "University list error: %s\n", err.c_str());
            return 1;
        }
    }
    if (cfg.emailDomain.empty() || app.auth.uni.count == 0)
        std::printf("NOTE: registration is disabled until email_domain in %s/app.cfg and %s are set.\n",
                    configDir.c_str(), uniPath.c_str());
    cafEnsureToday(app.caf);  // started on a new day: roll over before serving anyone
    saveAll(app);             // first start: writes the seed into data/

    app.email = makeSender(smtp, dataDir + "/outbox");
    bool outbox = std::string(app.email->modeName()) == "outbox";
    std::printf("Email mode: %s%s%s\n", app.email->modeName(),
                outbox ? " - SMTP not configured, emails are files in " : "", outbox ? (dataDir + "/outbox").c_str() : "");
    std::string placeholderFiles[] = {configDir + "/app.cfg", configDir + "/smtp.cfg", seedDir + "/menu.txt",
                                      seedDir + "/settings.txt"};
    printPlaceholders(placeholderFiles, 4);
    std::printf("Loaded %d menu items, %d users, %d reservations (%d ready awaiting reminders).\n",
                app.caf.menu.count, app.auth.users.count, app.caf.reservations.size, app.caf.reminders.count);

    // Lock file: tells cafeteria_admin the server is running.
    std::string lockPath = dataDir + "/server.lock";
#ifdef _WIN32
    atomicWrite(lockPath, std::to_string(_getpid()) + "\n", err);
    SetConsoleCtrlHandler(onCtrl, TRUE);
#endif

    httplib::Server svr;
    g_server = &svr;
    registerRoutes(svr, app, app.fakeClock != nullptr);
    if (!svr.set_mount_point("/", webDir)) std::printf("WARNING: web folder %s not found\n", webDir.c_str());

    // Timer thread: wakes every tickSeconds (proposal: every minute).
    std::thread timer;
    if (tickSeconds > 0) {
        timer = std::thread([&app, tickSeconds]() {
            int waited = 0;
            while (!g_stop) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (++waited < tickSeconds) continue;
                waited = 0;
                NotificationList out;
                nlInit(out);
                {
                    std::lock_guard<std::mutex> lock(app.mu);
                    timerTick(app.caf, out);
                    sessionPruneExpired(app.auth);
                    saveAll(app);
                }
                sendAll(app, out);
            }
        });
    }

    std::printf("Listening on http://%s:%d  (Ctrl+C to stop)\n", host.c_str(), port);
    std::fflush(stdout);
    bool ok = svr.listen(host, port);
    g_stop = true;
    if (timer.joinable()) timer.join();
    {
        std::lock_guard<std::mutex> lock(app.mu);
        saveAll(app);
    }
    std::remove(lockPath.c_str());
    if (!ok) {
        std::fprintf(stderr, "Could not listen on %s:%d\n", host.c_str(), port);
        return 1;
    }
    std::printf("Stopped.\n");
    return 0;
}
