// End-to-end test of the proposal's testing plan (section 12) against the real
// server binary: fake clock, temp data dir, emails written to the outbox.
//
//   e2e_section12 <path to cafeteria_server.exe> <project root>
//
// Prints PASS/FAIL per check and exits with the number of failures.
#include "auth/auth.h"
#include "auth/user_store.h"
#include "httplib.h"
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <windows.h>

namespace fs = std::filesystem;

static int g_failures = 0, g_checks = 0;
static void check(bool ok, const char* name, const std::string& detail = "") {
    g_checks++;
    if (ok) {
        std::printf("PASS %s\n", name);
    } else {
        g_failures++;
        std::printf("FAIL %s  %s\n", name, detail.c_str());
    }
    std::fflush(stdout);
}

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

// ---- server process ---------------------------------------------------------------

struct ServerProc {
    PROCESS_INFORMATION pi = {};
    bool running = false;
};

static int freePort() {
    httplib::Server probe;
    int port = probe.bind_to_any_port("127.0.0.1");
    probe.stop();
    return port;
}

static std::string g_exe, g_root, g_work, g_data, g_config;
static int g_port = 0;

static bool startServer(ServerProc& sp, const std::string& fakeTime) {
    std::string cmd = "\"" + g_exe + "\" --port " + std::to_string(g_port) + " --data \"" + g_data +
                      "\" --config \"" + g_config + "\" --seed \"" + g_root + "/seed\" --web \"" + g_root +
                      "/web\" --fake-clock " + fakeTime + " --tick-seconds 0";
    SECURITY_ATTRIBUTES sa = {sizeof sa, nullptr, TRUE};
    HANDLE log = CreateFileA((g_work + "/server.log").c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOA si = {};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = log;
    si.hStdError = log;
    si.hStdInput = nullptr;
    bool ok = CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                             &sp.pi) != 0;
    CloseHandle(log);
    if (!ok) return false;
    sp.running = true;
    httplib::Client c("127.0.0.1", g_port);
    for (int i = 0; i < 100; i++) {
        if (WaitForSingleObject(sp.pi.hProcess, 0) == WAIT_OBJECT_0) return false;  // exited at start-up
        auto r = c.Get("/api/health");
        if (r && r->status == 200) return true;
        Sleep(100);
    }
    return false;
}

static void killServer(ServerProc& sp) {
    if (!sp.running) return;
    TerminateProcess(sp.pi.hProcess, 1);  // simulates a crash / power cut
    WaitForSingleObject(sp.pi.hProcess, 5000);
    CloseHandle(sp.pi.hProcess);
    CloseHandle(sp.pi.hThread);
    sp.running = false;
}

// ---- a "browser" with one session cookie -------------------------------------------

struct Browser {
    httplib::Client cli{"127.0.0.1", g_port};
    std::string cookie;
    Browser() {
        // Fail a stuck request quickly and visibly instead of waiting 5 minutes.
        cli.set_connection_timeout(5, 0);
        cli.set_read_timeout(15, 0);
        cli.set_write_timeout(15, 0);
    }

    httplib::Result post(const std::string& path, const httplib::Params& params, bool withHeader = true) {
        httplib::Headers h;
        if (withHeader) h.emplace("X-Cafe", "1");
        if (!cookie.empty()) h.emplace("Cookie", cookie);
        auto r = cli.Post(path, h, params);
        remember(r);
        return r;
    }
    httplib::Result get(const std::string& path) {
        httplib::Headers h;
        if (!cookie.empty()) h.emplace("Cookie", cookie);
        auto r = cli.Get(path, h);
        remember(r);
        return r;
    }
    void remember(const httplib::Result& r) {
        if (!r) return;
        std::string sc = r->get_header_value("Set-Cookie");
        if (sc.rfind("session=", 0) == 0) cookie = sc.substr(0, sc.find(';'));
    }
};

static int status(const httplib::Result& r) { return r ? r->status : -1; }
static std::string body(const httplib::Result& r) {
    return r ? r->body : "(no response: " + httplib::to_string(r.error()) + ")";
}

// ---- outbox ---------------------------------------------------------------------

struct Mail {
    std::string seq, type, to, resId, subject;
};
static const int MAX_MAILS = 2000;
static Mail g_mails[MAX_MAILS];
static int g_mailCount = 0;

static void readOutbox() {
    g_mailCount = 0;
    std::ifstream in(g_data + "/outbox/index.log");
    std::string line;
    while (std::getline(in, line) && g_mailCount < MAX_MAILS) {
        std::string f[5];
        int n = 0;
        size_t start = 0;
        for (size_t i = 0; i <= line.size() && n < 5; i++) {
            if (i == line.size() || (line[i] == '|' && n < 4)) {
                f[n++] = line.substr(start, i - start);
                start = i + 1;
            }
        }
        if (n == 5) g_mails[g_mailCount++] = {f[0], f[1], f[2], f[3], f[4]};
    }
}

static int countMails(const char* type, const std::string& to, int fromIndex = 0) {
    readOutbox();
    int n = 0;
    for (int i = fromIndex; i < g_mailCount; i++)
        if (g_mails[i].type == type && (to.empty() || g_mails[i].to == to)) n++;
    return n;
}

static std::string latestCode(const std::string& to) {
    readOutbox();
    for (int i = g_mailCount - 1; i >= 0; i--) {
        if (g_mails[i].type == "verification" && g_mails[i].to == to) {
            std::ifstream in(g_data + "/outbox/" + g_mails[i].seq + "_verification.txt");
            std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            size_t p = all.find("code is ");
            return p == std::string::npos ? "" : all.substr(p + 8, 6);
        }
    }
    return "";
}

// JSON helpers: the API returns flat, predictable JSON.
static std::string jsonStr(const std::string& json, const std::string& key) {
    std::string k = "\"" + key + "\":\"";
    size_t p = json.find(k);
    if (p == std::string::npos) return "";
    p += k.size();
    return json.substr(p, json.find('"', p) - p);
}
static long jsonInt(const std::string& json, const std::string& key, size_t from = 0) {
    std::string k = "\"" + key + "\":";
    size_t p = json.find(k, from);
    if (p == std::string::npos) return -99999;
    return std::strtol(json.c_str() + p + k.size(), nullptr, 10);
}
// The "remaining" of a menu item by name.
static long remainingOf(const std::string& menuJson, const std::string& name) {
    size_t p = menuJson.find("\"name\":\"" + name + "\"");
    return p == std::string::npos ? -99999 : jsonInt(menuJson, "remaining", p);
}

// ---- setup ----------------------------------------------------------------------

static const char* PW = "studentpass1";
static std::string uniEmail(int i) { return "s" + std::to_string(i) + "@uni.example"; }
static std::string uniId(int i) { return "10000000000000" + std::string(i < 10 ? "0" : "") + std::to_string(i); }

static void writeFixtures() {
    // A server left over from an earlier, interrupted run would keep writing
    // into this folder: stop it first (its PID is in the lock file).
    {
        std::ifstream lock(g_data + "/server.lock");
        unsigned long pid = 0;
        if (lock >> pid && pid > 0) {
            HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, (DWORD)pid);
            if (h != nullptr) {
                TerminateProcess(h, 1);
                WaitForSingleObject(h, 5000);
                CloseHandle(h);
            }
        }
    }
    fs::remove_all(g_work);
    fs::create_directories(g_data);
    fs::create_directories(g_config);
    std::ofstream app(g_config + "/app.cfg");
    app << "email_domain=uni.example\nsession_minutes=480\nlockout_failures=5\nlockout_minutes=15\n"
        << "code_minutes=15\ncooking_capacity=100\n";
    std::ofstream uni(g_config + "/university_list.csv");
    uni << "id,email,role,name,department\n"
        << "1072520005101030,ayon@uni.example,student,Ayon Das,CSE\n"
        << "1072520005101050,ali@uni.example,student,Mohammad Ali Gazi,CSE\n"
        << "2000000000000001,teacher1@uni.example,teacher,Sample Teacher,EEE\n";
    for (int i = 1; i <= 12; i++) uni << uniId(i) << "," << uniEmail(i) << ",student,Student " << i << ",CSE\n";
    std::ofstream smtp(g_config + "/smtp.cfg");
    smtp << "host=[CONFIRM]\nsender=[CONFIRM]\n";
    // Staff accounts are made by the administrator tool, never through the web.
    FakeClock clock;
    AuthState a;
    authInit(a, &clock, authDefaults());
    addStaff(a, "cafe.staff", "Cafeteria Staff", "staffpass123");
    std::string err;
    saveUsers(a.users, g_data, err);
    authFree(a);
}

static bool registerUser(Browser& b, const std::string& email, const std::string& id) {
    auto r = b.post("/api/register", {{"email", email}, {"id", id}, {"password", PW}});
    if (status(r) != 200) return false;
    r = b.post("/api/verify", {{"email", email}, {"code", latestCode(email)}, {"password", PW}});
    if (status(r) != 200) return false;
    r = b.post("/api/login", {{"email", email}, {"password", PW}});
    return status(r) == 200;
}

static std::string reserveRes(Browser& b, int item, const std::string& qty) {
    return body(b.post("/api/reservations", {{"item", std::to_string(item)}, {"quantity", qty}}));
}

const int KHICHURI = 0, FRIED_RICE = 1, TEHARI = 2, LAYS = 5;  // indices in seed/menu.txt

static int realMain(int argc, char** argv);

int main(int argc, char** argv) {
    try {
        return realMain(argc, argv);
    } catch (const std::exception& e) {
        std::printf("FAIL uncaught exception: %s\n", e.what());
        std::fflush(stdout);
        return 99;
    }
}

static int realMain(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: e2e_section12 <server exe> <project root>\n");
        return 2;
    }
    g_exe = argv[1];
    g_root = argv[2];
    g_work = (fs::temp_directory_path() / "cafeteria_e2e").string();
    g_data = g_work + "/data";
    g_config = g_work + "/config";
    writeFixtures();
    g_port = freePort();

    ServerProc server;
    if (!startServer(server, "2026-10-23T08:00")) {
        std::printf("FAIL server did not start (see %s/server.log)\n", g_work.c_str());
        return 1;
    }

    Browser anon, ayon, ali, teacher, staff, s[13];

    // ---- 12.1 registration ------------------------------------------------------
    auto r = anon.post("/api/register", {{"email", "ayon@gmail.com"}, {"id", "1072520005101030"}, {"password", PW}});
    check(status(r) == 400 && has(body(r), "university email"), "12.1 non-university email rejected", body(r));
    r = anon.post("/api/register", {{"email", "ayon@uni.example"}, {"id", "1072520009999999"}, {"password", PW}});
    check(status(r) == 400 && has(body(r), "not on the university"), "12.1 unlisted ID rejected", body(r));
    r = ayon.post("/api/register", {{"email", "ayon@uni.example"}, {"id", "1072520005101030"}, {"password", PW}});
    check(status(r) == 200 && latestCode("ayon@uni.example").size() == 6, "12.1 valid registration sends a code",
          body(r));
    std::string code = latestCode("ayon@uni.example");
    r = ayon.post("/api/verify",
                  {{"email", "ayon@uni.example"}, {"code", code == "000000" ? "111111" : "000000"}, {"password", PW}});
    check(status(r) == 400, "12.1 wrong verification code rejected", body(r));
    r = ayon.post("/api/verify", {{"email", "ayon@uni.example"}, {"code", code}, {"password", "someone-elses-pw"}});
    check(status(r) == 400, "review: verify with a different password refused (no takeover)", body(r));
    r = ayon.post("/api/register", {{"email", "ayon@uni.example"}, {"id", "1072520005101030"}, {"password", PW}});
    code = latestCode("ayon@uni.example");
    r = ayon.post("/api/verify", {{"email", "ayon@uni.example"}, {"code", code}, {"password", PW}});
    check(status(r) == 200, "12.1 correct verification code accepted", body(r));
    r = ayon.post("/api/login", {{"email", "ayon@uni.example"}, {"password", PW}});
    check(status(r) == 200 && !ayon.cookie.empty(), "12.1 verified student can log in", body(r));
    std::string sc = r ? r->get_header_value("Set-Cookie") : "";
    check(has(sc, "HttpOnly") && has(sc, "SameSite=Strict"), "security: session cookie is HttpOnly + SameSite", sc);

    // ---- 12.2 roles and interfaces --------------------------------------------------
    check(registerUser(ali, "ali@uni.example", "1072520005101050"), "setup: ali registered");
    check(registerUser(teacher, "teacher1@uni.example", "2000000000000001"), "setup: teacher registered");
    r = ayon.get("/api/me");
    check(jsonStr(body(r), "role") == "student" && jsonStr(body(r), "universityId") == "1072520005101030",
          "12.2 student sees student profile (university ID)", body(r));
    r = teacher.get("/api/me");
    check(jsonStr(body(r), "role") == "teacher" && jsonStr(body(r), "department") == "EEE",
          "12.2 teacher sees teacher profile (department)", body(r));
    r = staff.post("/api/login", {{"email", "cafe.staff"}, {"password", "staffpass123"}});
    check(status(r) == 401, "12.2 staff cannot use the public login", body(r));
    r = staff.post("/api/staff/login", {{"login", "cafe.staff"}, {"password", "staffpass123"}});
    check(status(r) == 200, "12.2 staff log in through the staff portal", body(r));
    r = ayon.post("/api/staff/login", {{"login", "ayon@uni.example"}, {"password", PW}});
    check(status(r) == 401, "12.2 student cannot use the staff login", body(r));
    r = ayon.get("/staff/portal.html");
    check(status(r) == 302 && has(r->get_header_value("Location"), "/staff/login.html"),
          "12.2 student opening the staff portal page is redirected", std::to_string(status(r)));
    r = staff.get("/staff/portal.html");
    check(status(r) == 200 && has(body(r), "<html"), "12.2 staff can open the staff portal page");
    r = ayon.post("/api/staff/stock", {{"item", std::to_string(LAYS)}, {"value", "50"}});
    check(status(r) == 403, "review: student_cannot_call_staff_api", body(r));
    r = ayon.get("/api/staff/reservations");
    check(status(r) == 403, "review: student cannot list all reservations", body(r));
    r = staff.get("/api/menu");
    check(status(r) == 403, "12.2 staff do not use the student menu API", body(r));
    r = anon.get("/api/menu");
    check(status(r) == 401, "security: menu needs a login", body(r));
    r = ayon.post("/api/reservations", {{"item", "2"}, {"quantity", "1"}}, false);
    check(status(r) == 403, "security: POST without X-Cafe header refused (CSRF)", body(r));

    // ---- 12.3 reserve ------------------------------------------------------------------
    int mailsBefore = (readOutbox(), g_mailCount);
    std::string res = reserveRes(ayon, TEHARI, "1");
    check(has(res, "\"ok\":true") && jsonStr(res, "code") == "R-0001", "12.3 valid reservation confirmed", res);
    r = ayon.get("/api/reservations/mine");
    check(has(body(r), "\"code\":\"R-0001\"") && has(body(r), "\"status\":\"Reserved\""),
          "12.3 reservation appears in My Reservations", body(r));
    check(countMails("confirmation", "ayon@uni.example", mailsBefore) == 1, "12.3 confirmation email sent");
    r = ali.get("/api/reservations/mine");
    check(!has(body(r), "R-0001"), "privacy: other users do not see the reservation", body(r));

    // ---- 12.4 sold out and per-user limit -----------------------------------------------
    r = staff.post("/api/staff/limit", {{"item", std::to_string(FRIED_RICE)}, {"value", "1"}});
    check(status(r) == 200, "setup: staff set Fried Rice limit to 1", body(r));
    res = reserveRes(ali, FRIED_RICE, "1");
    check(has(res, "\"ok\":true"), "12.4 last Fried Rice portion reserved", res);
    std::string aliFriedRice = std::to_string(jsonInt(res, "id"));
    r = teacher.post("/api/reservations", {{"item", std::to_string(FRIED_RICE)}, {"quantity", "1"}});
    check(status(r) == 409 && has(body(r), "Not enough portions"), "12.4 sold-out item rejected", body(r));
    r = ayon.post("/api/reservations", {{"item", std::to_string(TEHARI)}, {"quantity", "2"}});
    check(status(r) == 409 && has(body(r), "per-person limit"), "12.4 more than the per-user limit rejected",
          body(r));
    r = ayon.post("/api/reservations", {{"item", std::to_string(LAYS)}, {"quantity", "1"}});
    check(status(r) == 400 && has(body(r), "view-only"), "12.4 packaged item cannot be reserved", body(r));

    // ---- 12.5 cancel before the deadline ------------------------------------------------
    long before = remainingOf(body(ali.get("/api/menu")), "Khichuri");
    res = reserveRes(ali, KHICHURI, "1");
    std::string aliKhichuri = std::to_string(jsonInt(res, "id"));
    long during = remainingOf(body(ali.get("/api/menu")), "Khichuri");
    r = ayon.post("/api/reservations/cancel", {{"id", aliKhichuri}});
    check(status(r) == 404, "review: cannot_cancel_others", body(r));
    r = ali.post("/api/reservations/cancel", {{"id", aliKhichuri}});
    long after = remainingOf(body(ali.get("/api/menu")), "Khichuri");
    check(status(r) == 200 && during == before - 1 && after == before,
          "12.5 cancel before deadline returns the portion",
          std::to_string(before) + "/" + std::to_string(during) + "/" + std::to_string(after));

    // ---- addition #1: waitlist for a sold-out item ------------------------------------------
    const std::string FR = std::to_string(FRIED_RICE);
    r = teacher.post("/api/waitlist/join", {{"item", FR}, {"quantity", "1"}});
    check(status(r) == 200 && jsonInt(body(r), "position") == 1, "waitlist: join sold-out Fried Rice at position 1",
          body(r));
    r = teacher.post("/api/waitlist/join", {{"item", FR}, {"quantity", "1"}});
    check(status(r) == 409 && has(body(r), "already on the waitlist"), "waitlist: joining twice refused", body(r));
    r = ayon.post("/api/waitlist/join", {{"item", std::to_string(TEHARI)}, {"quantity", "1"}});
    check(status(r) == 409 && has(body(r), "still available"), "waitlist: cannot join while portions remain", body(r));
    {
        std::string m = body(teacher.get("/api/menu"));
        size_t frPos = m.find("\"name\":\"Fried Rice\"");
        check(jsonInt(m, "yourPosition", frPos) == 1 && jsonInt(m, "waiting", frPos) == 1,
              "waitlist: menu shows the person's place in line", m);
    }
    readOutbox();
    int wlFrom = g_mailCount;
    r = ali.post("/api/reservations/cancel", {{"id", aliFriedRice}});
    check(status(r) == 200 && countMails("confirmation", "teacher1@uni.example", wlFrom) == 1,
          "waitlist: a cancellation gives the portion to the first person waiting, with email", body(r));
    r = teacher.get("/api/reservations/mine");
    check(has(body(r), "\"item\":\"Fried Rice\"") && remainingOf(body(ali.get("/api/menu")), "Fried Rice") == 0,
          "waitlist: promoted person now holds the reservation; portion used once", body(r));

    // ---- review: concurrent last portion --------------------------------------------------
    for (int i = 1; i <= 12; i++) registerUser(s[i], uniEmail(i), uniId(i));
    staff.post("/api/staff/limit", {{"item", std::to_string(KHICHURI)}, {"value", "1"}});
    std::atomic<int> okCount(0), soldOut(0);
    std::thread threads[10];
    for (int i = 0; i < 10; i++) {
        threads[i] = std::thread([&, i]() {
            std::string b = reserveRes(s[i + 1], KHICHURI, "1");
            if (has(b, "\"ok\":true")) okCount++;
            else if (has(b, "Not enough portions")) soldOut++;
        });
    }
    for (auto& t : threads) t.join();
    check(okCount == 1 && soldOut == 9, "review: concurrent_last_portion (10 at once, 1 left)",
          std::to_string(okCount.load()) + " ok, " + std::to_string(soldOut.load()) + " sold out");
    r = s[6].post("/api/waitlist/join", {{"item", std::to_string(KHICHURI)}, {"quantity", "1"}});
    check(status(r) == 200, "waitlist: s6 joins the sold-out Khichuri waitlist", body(r));

    // ---- FIFO scenario on Tehari: ayon(R-0001), teacher, s11 (cancels), s12 ------------
    reserveRes(teacher, TEHARI, "1");
    res = reserveRes(s[11], TEHARI, "1");
    std::string s11Id = std::to_string(jsonInt(res, "id"));
    res = reserveRes(s[12], TEHARI, "1");
    std::string s12Id = std::to_string(jsonInt(res, "id"));
    s[11].post("/api/reservations/cancel", {{"id", s11Id}});

    // ---- cutoff boundary: advance the fake clock 08:00 -> 09:59 -> 10:00 ---------------------
    r = staff.post("/api/dev/advance", {{"minutes", "119"}});
    check(jsonStr(body(r), "now") == "09:59", "setup: clock at 09:59", body(r));
    res = reserveRes(s[3], TEHARI, "1");
    check(has(res, "\"ok\":true"), "12.4 reservation at 09:59 accepted", res);
    std::string s3Id = std::to_string(jsonInt(res, "id"));
    staff.post("/api/dev/advance", {{"minutes", "1"}});
    r = s[4].post("/api/reservations", {{"item", std::to_string(TEHARI)}, {"quantity", "1"}});
    check(status(r) == 409 && has(body(r), "cutoff"), "12.4 reservation at 10:00 (cutoff) rejected", body(r));
    check(countMails("waitlist-closed", "s6@uni.example") == 1 &&
              countMails("waitlist-closed", "teacher1@uni.example") == 0,
          "waitlist: closes at the cutoff with one email to each person still waiting");
    long tehariBefore = remainingOf(body(s[12].get("/api/menu")), "Tehari");
    r = s[12].post("/api/reservations/cancel", {{"id", s12Id}});
    long tehariAfter = remainingOf(body(s[12].get("/api/menu")), "Tehari");
    check(status(r) == 409 && has(body(r), "deadline") && tehariAfter == tehariBefore,
          "12.5 cancel after deadline refused, portion not returned", body(r));
    s[3].post("/api/reservations/cancel", {{"id", s3Id}});  // also refused (after deadline)

    // ---- 12.6 ready emails FIFO, skipping cancelled -----------------------------------------
    readOutbox();
    int readyFrom = g_mailCount;
    r = staff.post("/api/staff/ready", {{"item", std::to_string(TEHARI)}, {"portions", "0"}});
    check(jsonInt(body(r), "madeReady") == 4 && jsonInt(body(r), "skipped") == 1, "12.6 mark ready: 4 ready, 1 skipped",
          body(r));
    readOutbox();
    std::string order;
    for (int i = readyFrom; i < g_mailCount; i++)
        if (g_mails[i].type == "ready") order += g_mails[i].to + " ";
    check(order == "ayon@uni.example teacher1@uni.example s12@uni.example s3@uni.example ",
          "12.6 ready emails in FIFO order, cancelled skipped", order);
    check(countMails("ready", "s11@uni.example") == 0, "12.6 cancelled reservation gets no ready email");

    // ---- 12.7 reminders, collected, restart, expiry --------------------------------------------
    staff.post("/api/dev/advance", {{"minutes", "60"}});  // 11:00
    check(countMails("reminder", "ayon@uni.example") == 1 && countMails("reminder", "teacher1@uni.example") == 1,
          "12.7 first hourly reminder at 11:00");
    r = staff.get("/api/staff/reservations?q=R-0001");
    check(has(body(r), "\"code\":\"R-0001\"") && !has(body(r), "R-0002"), "staff search by reservation ID", body(r));
    r = staff.get("/api/staff/reservations?q=R-0001' OR 1=1");
    check(status(r) == 400, "12.9 special characters in search rejected", body(r));
    r = staff.post("/api/staff/collected", {{"id", "1"}});
    check(status(r) == 200, "12.7 staff mark R-0001 collected", body(r));
    staff.post("/api/dev/advance", {{"minutes", "60"}});  // 12:00
    check(countMails("reminder", "ayon@uni.example") == 1 && countMails("reminder", "teacher1@uni.example") == 2,
          "12.7 reminders stop after collection, continue for others");

    // ---- 12.8 undo a staff action -----------------------------------------------------------------
    r = staff.post("/api/staff/stock", {{"item", std::to_string(LAYS)}, {"value", "25"}});
    check(status(r) == 200, "setup: staff set Lays stock 25", body(r));
    r = staff.post("/api/staff/undo", {});
    std::string ov = body(staff.get("/api/staff/overview"));
    size_t laysPos = ov.find("\"name\":\"Lays\"");
    // '>' arrives JSON-escaped as > (the server escapes < and > in all JSON text)
    check(status(r) == 200 && has(body(r), "Set Lays stock 0 -\\u003e 25") && jsonInt(ov, "stock", laysPos) == 0,
          "12.8 undo restores Lays stock", body(r));

    // ---- 12.10 restart: crash the server at 12:00 and start it again -------------------------
    killServer(server);
    check(startServer(server, "2026-10-23T12:00"), "12.10 server restarts on the same data");
    r = ayon.get("/api/me");
    check(status(r) == 401, "12.10 sessions are not kept across restart (log in again)", body(r));
    {
        std::ifstream lk(g_data + "/server.lock");
        std::string before((std::istreambuf_iterator<char>(lk)), std::istreambuf_iterator<char>());
        int savedPort = g_port;
        g_port = freePort();
        ServerProc second;
        bool started = startServer(second, "2026-10-23T12:00");
        DWORD code = STILL_ACTIVE;
        if (second.running) {
            WaitForSingleObject(second.pi.hProcess, 5000);
            GetExitCodeProcess(second.pi.hProcess, &code);
        }
        killServer(second);
        g_port = savedPort;
        std::ifstream lk2(g_data + "/server.lock");
        std::string after((std::istreambuf_iterator<char>(lk2)), std::istreambuf_iterator<char>());
        httplib::Client c("127.0.0.1", g_port);
        auto h = c.Get("/api/health");
        check(!started && code == 1 && !before.empty() && after == before && h && h->status == 200,
              "review: second server on the same data folder refuses, first keeps its lock",
              "exit=" + std::to_string(code) + " lock " + before + " -> " + after);
    }
    ayon.post("/api/login", {{"email", "ayon@uni.example"}, {"password", PW}});
    teacher.post("/api/login", {{"email", "teacher1@uni.example"}, {"password", PW}});
    staff.post("/api/staff/login", {{"login", "cafe.staff"}, {"password", "staffpass123"}});
    r = ayon.get("/api/reservations/mine");
    check(has(body(r), "\"code\":\"R-0001\"") && has(body(r), "\"status\":\"Collected\""),
          "12.10 users and reservations restored", body(r));
    r = ali.post("/api/login", {{"email", "ali@uni.example"}, {"password", PW}});
    std::string menu = body(ali.get("/api/menu"));
    check(remainingOf(menu, "Fried Rice") == 0 && remainingOf(menu, "Khichuri") == 0, "12.10 menu and portions restored",
          menu);
    r = staff.post("/api/staff/undo", {});
    check(status(r) == 409 && has(body(r), "no staff action"), "12.8/12.10 stack empty after restart: nothing to undo",
          body(r));
    readOutbox();
    int afterRestart = g_mailCount;
    staff.post("/api/dev/advance", {{"minutes", "300"}});  // 17:00 = 7 h after ready
    check(countMails("ready", "", afterRestart) == 0, "12.10 no duplicate ready emails after restart");
    check(countMails("reminder", "teacher1@uni.example") == 6, "12.7 six hourly reminders in the 7-hour window",
          std::to_string(countMails("reminder", "teacher1@uni.example")));
    check(countMails("expired", "teacher1@uni.example") == 1 && countMails("expired", "ayon@uni.example") == 0,
          "12.7 expiry email when the window ends (not for collected food)");
    r = teacher.get("/api/reservations/mine");
    check(has(body(r), "\"status\":\"Expired\""), "12.7 uncollected reservation is Expired", body(r));

    // ---- 12.9 invalid input ---------------------------------------------------------------------
    r = ayon.post("/api/reservations", {{"item", std::to_string(TEHARI)}, {"quantity", "-1"}});
    check(status(r) == 400, "12.9 negative quantity rejected", body(r));
    r = ayon.post("/api/reservations", {{"item", std::to_string(TEHARI)}, {"quantity", "abc"}});
    check(status(r) == 400, "12.9 non-numeric quantity rejected", body(r));
    r = ayon.post("/api/reservations", {{"item", "999"}, {"quantity", "1"}});
    check(status(r) == 400, "12.9 unknown item rejected", body(r));
    r = anon.post("/api/register", {{"email", std::string(5000, 'a') + "@uni.example"}, {"id", "1"}, {"password", PW}});
    check(status(r) == 400 || status(r) == 413, "12.9 very long text rejected", std::to_string(status(r)));
    r = anon.post("/api/login", {{"email", "<script>alert(1)</script>"}, {"password", "x"}});
    check(status(r) == 401 && !has(body(r), "<script>"), "12.9 special characters handled safely", body(r));
    r = staff.post("/api/staff/settings", {{"key", "cutoff"}, {"value", "25:99"}});
    check(status(r) == 400, "12.9 invalid time setting rejected", body(r));
    Browser attacker;
    int lastStatus = 0;
    std::string lastBody;
    for (int i = 0; i < 5; i++) {
        r = attacker.post("/api/login", {{"email", "ali@uni.example"}, {"password", "wrong-password"}});
        lastStatus = status(r);
        lastBody = body(r);
    }
    r = attacker.post("/api/login", {{"email", "ali@uni.example"}, {"password", PW}});
    check(lastStatus == 423 && status(r) == 423 && has(body(r), "locked"),
          "12.9 wrong passwords lock the account", lastBody);

    // ---- settings changed by staff -------------------------------------------------------------
    r = staff.post("/api/staff/settings", {{"key", "cutoff"}, {"value", "18:00"}});
    check(status(r) == 200, "staff change cutoff to 18:00", body(r));
    s[5].post("/api/login", {{"email", uniEmail(5)}, {"password", PW}});  // sessions ended at the restart
    res = reserveRes(s[5], TEHARI, "1");
    check(has(res, "\"ok\":true"), "reservation allowed again after cutoff moved", res);

    // ---- addition #2: email retry when sending fails ----------------------------------------
    // Block the outbox (a file where the folder should be) so every send fails.
    killServer(server);
    fs::rename(g_data + "/outbox", g_data + "/outbox_saved");
    { std::ofstream blocker(g_data + "/outbox"); blocker << "blocked"; }
    check(startServer(server, "2026-10-23T17:10"), "setup: restart with the email outbox blocked");
    staff.post("/api/staff/login", {{"login", "cafe.staff"}, {"password", "staffpass123"}});
    s[8].post("/api/login", {{"email", uniEmail(8)}, {"password", PW}});
    res = reserveRes(s[8], TEHARI, "1");
    std::string ov2 = body(staff.get("/api/staff/overview"));
    check(has(res, "\"ok\":true") && jsonInt(ov2, "emailsWaiting") == 1,
          "email retry: a failed confirmation waits in the retry queue", res + " " + ov2.substr(0, 200));
    fs::remove(g_data + "/outbox");
    fs::rename(g_data + "/outbox_saved", g_data + "/outbox");
    staff.post("/api/dev/advance", {{"minutes", "1"}});
    ov2 = body(staff.get("/api/staff/overview"));
    check(jsonInt(ov2, "emailsWaiting") == 0 && countMails("confirmation", "s8@uni.example") == 1,
          "email retry: sent one minute later once the outbox works again",
          ov2.substr(0, 120) + " ... emailsWaiting=" + std::to_string(jsonInt(ov2, "emailsWaiting")) +
              " s8-confirmations=" + std::to_string(countMails("confirmation", "s8@uni.example")) +
              " outbox-is-dir=" + std::to_string(fs::is_directory(g_data + "/outbox")));

    std::printf("(stopping server)\n");
    std::fflush(stdout);
    killServer(server);
    std::printf("(server stopped)\n");
    std::fflush(stdout);
    std::printf("\n%d checks, %d passed, %d failed\n", g_checks, g_checks - g_failures, g_failures);
    std::fflush(stdout);
    return g_failures;
}
