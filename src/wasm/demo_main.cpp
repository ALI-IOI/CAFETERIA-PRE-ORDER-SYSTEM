// WebAssembly demo of the cafeteria server (GitHub Pages live demo).
//
// The same C++ as the real server: the hand-built data structures, the rules in
// src/core, the storage files, Argon2 password hashing (libsodium) and every route
// in src/server/routes.cpp. Only the transport differs: there is no socket, so the
// browser (web-demo/cafe-worker.js) passes each /api request to cafe_request() and
// shows the answer. Differences from the real server, all for the demo:
//   - data files live in the browser (IndexedDB), one private cafeteria per browser;
//   - the clock is a fake clock that starts at 08:30 on the visitor's first visit and
//     then moves with real time (the demo bar can move it forward);
//   - emails go to data/outbox, shown in the demo bar's inbox;
//   - sessions are written to data/demo_sessions.txt so moving between pages keeps
//     you logged in (each page load restarts this program);
//   - demo accounts are created on the first visit.
#include "auth/crypto.h"
#include "auth/user_store.h"
#include "httplib.h"
#include "server/app.h"
#include "server/json.h"
#include "server/routes.h"
#include "storage/file_util.h"
#include "storage/storage.h"
#include <emscripten/emscripten.h>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

static const char* const DATA = "/data";
static const char* const CONFIG = "/demo/config";
static const char* const SEED = "/seed";

static AppState* g_app = nullptr;
static FakeClock g_clock;
static httplib::Server* g_svr = nullptr;
static httplib::MultipartFormData g_parts;  // filled by cafe_add_part() before an upload
static std::string g_out;                   // last answer handed to JavaScript

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

// ---- sessions and clock survive a page load -------------------------------------------

static void saveDemoState(double realNow) {
    std::string s;
    for (Session* x = g_app->auth.sessions.head; x; x = x->next)  // head (newest) first
        s += x->token + "|" + x->login + "|" + std::to_string((int)x->role) + "|" +
             std::to_string((long long)x->expiresAt) + "\n";
    std::string err;
    atomicWrite(std::string(DATA) + "/demo_sessions.txt", s, err);
    atomicWrite(std::string(DATA) + "/demo_clock.txt",
                std::to_string((long long)g_clock.t) + " " + std::to_string((long long)realNow) + "\n", err);
}

static void loadDemoSessions() {
    std::ifstream in(std::string(DATA) + "/demo_sessions.txt");
    std::string line, f[4];
    Session* tail = nullptr;
    while (std::getline(in, line)) {
        if (splitFields(line, '|', f, 4) != 4) continue;
        int role;
        long long exp;
        if (!parseIntStrict(f[2], role) || !parseIntStrict(f[3], exp) || role < 0 || role > 3) continue;
        Session* s = new Session{f[0], f[1], (Role)role, (time_t)exp, nullptr};
        if (tail) tail->next = s;
        else g_app->auth.sessions.head = s;
        tail = s;
        g_app->auth.sessions.count++;
    }
    sessionPruneExpired(g_app->auth);
}

// One minute of the server's timer thread.
static void tickOnce(NotificationList& out) {
    g_clock.advanceMinutes(1);
    timerTick(g_app->caf, out);
}

// ---- demo accounts (first visit only) --------------------------------------------------

static void registerDemo(const char* email, const char* id, const char* password) {
    RegisterHashes h;
    NotificationList out;
    nlInit(out);
    if (makeRegisterHashes(password, h) &&
        registerStart(g_app->auth, email, id, password, out, &h) == AuthResult::Ok)
        registerVerify(g_app->auth, email, h.code, password);
    nlClear(out);  // the verification email is not needed for these
}

static void createDemoAccounts() {
    addStaff(g_app->auth, "demo.staff", "Demo Staff", "demo-staff-pass");
    registerDemo("ayon@uni.example", "1072520005101030", "demo-student-pass");
    registerDemo("teacher1@uni.example", "2000000000000001", "demo-teacher-pass");
    registerDemo("guard1@uni.example", "3000000000000002", "demo-guard-pass");
    addPerson(g_app->auth, "3000000000000001", "rahim@example.com", "demo-driver-pass");
}

// Two sample coupons, so a visitor can try one at the tray (staff can change them).
static void createDemoCoupons() {
    Coupon c;
    c.code = "WELCOME10";
    c.occasion = "Demo welcome week";
    c.kind = DiscountKind::Percent;
    c.value = 10;
    c.audience = "everyone";
    c.fromDay = c.toDay = c.maxUses = c.uses = 0;
    c.active = true;
    addCoupon(g_app->caf, "demo.staff", c);
    c.code = "TEACH50";
    c.occasion = "Teachers' Day";
    c.kind = DiscountKind::Fixed;
    c.value = 50;
    c.audience = "teacher";
    addCoupon(g_app->caf, "demo.staff", c);
}

// ---- form decoding -------------------------------------------------------------------

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i + 2 < s.size() && hexDigit(s[i + 1]) >= 0 && hexDigit(s[i + 2]) >= 0) {
            out += (char)(hexDigit(s[i + 1]) * 16 + hexDigit(s[i + 2]));
            i += 2;
        } else out += s[i];
    }
    return out;
}

static void addParams(httplib::Params& params, const std::string& text) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t amp = text.find('&', pos);
        if (amp == std::string::npos) amp = text.size();
        std::string pair = text.substr(pos, amp - pos);
        if (!pair.empty()) {
            size_t eq = pair.find('=');
            params.emplace(urlDecode(pair.substr(0, eq)), eq == std::string::npos ? "" : urlDecode(pair.substr(eq + 1)));
        }
        pos = amp + 1;
    }
}

// ---- exported to JavaScript ----------------------------------------------------------

extern "C" {

// Starts the cafeteria from the files in /data (already loaded from IndexedDB).
// realNow: seconds since 1970 now; y/mo/d: the visitor's date, used on a first visit.
EMSCRIPTEN_KEEPALIVE int cafe_boot(int y, int mo, int d, double realNow) {
    static AppState app;
    g_app = &app;
    std::string err;
    AppConfig cfg;
    SmtpConfig smtp;
    if (!loadAppConfig(std::string(CONFIG) + "/app.cfg", cfg, err)) {
        std::fprintf(stderr, "Config error: %s\n", err.c_str());
        return 1;
    }
    std::string clockFile = std::string(DATA) + "/demo_clock.txt";
    bool firstVisit = !fileExists(std::string(DATA) + "/users.txt");
    long long fakeT = 0, savedReal = 0;
    if (fileExists(clockFile) && std::sscanf(readFile(clockFile).c_str(), "%lld %lld", &fakeT, &savedReal) == 2) {
        g_clock.t = (time_t)fakeT;
    } else {
        g_clock.set(y, mo, d, 8, 30, 0);
        savedReal = (long long)realNow;
    }
    app.clock = &g_clock;
    app.fakeClock = &g_clock;
    app.dataDir = DATA;
    if (!cryptoInit()) return 1;
    cafInit(app.caf, app.clock, cfg.cookingCapacity);
    AuthConfig ac = authDefaults();
    ac.emailDomain = cfg.emailDomain;
    ac.sessionMinutes = cfg.sessionMinutes;
    ac.lockoutFailures = cfg.lockoutFailures;
    ac.lockoutMinutes = cfg.lockoutMinutes;
    ac.codeMinutes = cfg.codeMinutes;
    authInit(app.auth, app.clock, ac);
    if (!ensureDir(DATA, err) || !loadCafeteria(app.caf, DATA, SEED, err) || !loadUsers(app.auth.users, DATA, err) ||
        !uniLoadCsv(app.auth.uni, std::string(CONFIG) + "/university_list.csv", err) ||
        !staffComputersLoad(app.staffComputers, std::string(CONFIG) + "/staff_computers.txt", err)) {
        std::fprintf(stderr, "Cannot load data: %s\n", err.c_str());
        return 1;
    }
    app.email = makeSender(smtp, std::string(DATA) + "/outbox");
    mailerInit(app.mailer, app.email, app.clock, DATA);
    if (!mailerLoad(app.mailer, err)) return 1;
    if (firstVisit) {
        createDemoAccounts();
        createDemoCoupons();
    }
    loadDemoSessions();

    // The time the browser was away, as the server's timer would have seen it (up to a day).
    long long away = ((long long)realNow - savedReal) / 60;
    if (away > 24 * 60) away = 24 * 60;
    NotificationList out;
    nlInit(out);
    for (long long i = 0; i < away; i++) tickOnce(out);
    cafEnsureToday(app.caf);
    saveAll(app);
    sendAll(app, out);

    static httplib::Server svr;
    g_svr = &svr;
    registerRoutes(svr, app, true);  // true: POST /api/dev/advance moves the demo clock
    saveDemoState(realNow);
    return 0;
}

// A part of the next multipart upload (staff picture form).
EMSCRIPTEN_KEEPALIVE void cafe_add_part(const char* name, const char* filename, const char* type,
                                        const char* data, int len) {
    if (filename && filename[0]) {
        httplib::FormFile f{name, std::string(data, (size_t)len), filename, type};
        g_parts.files.emplace(name, f);
    } else {
        g_parts.fields.emplace(name, std::string(data, (size_t)len));
    }
}

// One HTTP request. headers: "Name: value" lines. Returns
// {"status":200,"headers":[["Name","value"],...],"body":"..."}.
EMSCRIPTEN_KEEPALIVE const char* cafe_request(const char* method, const char* path, const char* query,
                                              const char* headers, const char* body, int bodyLen, double realNow) {
    httplib::Request req;
    httplib::Response res;
    req.method = method;
    req.path = path;
    req.body = std::string(body, (size_t)bodyLen);
    req.remote_addr = "127.0.0.1";  // the browser is "this computer" for the staff terminal check
    std::istringstream hs(headers);
    std::string line;
    while (std::getline(hs, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string v = line.substr(colon + 1);
        while (!v.empty() && v[0] == ' ') v.erase(0, 1);
        req.headers.emplace(line.substr(0, colon), v);
    }
    req.headers.emplace("Host", "localhost");
    req.headers.emplace("Content-Length", std::to_string(req.body.size()));
    addParams(req.params, query);
    if (req.get_header_value("Content-Type").rfind("application/x-www-form-urlencoded", 0) == 0)
        addParams(req.params, req.body);
    if (req.is_multipart_form_data()) {
        req.form = g_parts;
        for (const auto& f : g_parts.fields) req.params.emplace(f.first, f.second);
    }
    g_parts = httplib::MultipartFormData();

    g_svr->dispatch(req, res);
    saveDemoState(realNow);

    Json j;
    j.beginObject().key("status").value(res.status).key("headers").beginArray();
    for (const auto& h : res.headers) j.beginArray().value(h.first).value(h.second).endArray();
    j.endArray().key("body").value(res.body).endObject();
    g_out = j.str();
    return g_out.c_str();
}

// The server's minute timer, run by the worker once a real minute.
EMSCRIPTEN_KEEPALIVE void cafe_tick(double realNow) {
    NotificationList out;
    nlInit(out);
    {
        std::lock_guard<std::mutex> lock(g_app->mu);
        tickOnce(out);
        sessionPruneExpired(g_app->auth);
        saveAll(*g_app);
    }
    sendAll(*g_app, out);
    mailerRetryDue(g_app->mailer);
    saveDemoState(realNow);
}

// The demo clock, e.g. "Sat 7 Nov 08:30".
EMSCRIPTEN_KEEPALIVE const char* cafe_now() {
    char buf[64];
    std::tm tmv = {};
    time_t t = g_clock.t;
    localtime_r(&t, &tmv);
    std::strftime(buf, sizeof buf, "%a %e %b %H:%M", &tmv);
    g_out = buf;
    return g_out.c_str();
}

}  // extern "C"
