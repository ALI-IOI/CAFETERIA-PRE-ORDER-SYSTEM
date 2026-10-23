// HTTP routes. Every handler runs through handle(), which:
//   1. refuses POSTs without the X-Cafe header (a cross-site form cannot set it),
//   2. locks the global mutex,
//   3. looks up the session cookie and checks the role on the server,
//   4. runs the handler (core functions on the hand-built structures),
//   5. saves the data files if anything changed,
//   6. unlocks, then sends the queued emails.
#include "server/routes.h"
#include "auth/validation.h"
#include "server/json.h"
#include "storage/file_util.h"
#include "storage/image_store.h"
#include "storage/history.h"

static const size_t SMALL_BODY_LIMIT = 4096;
static const char* const IMAGE_UPLOAD_PATH = "/api/staff/items/image";

// "/images/<file>" for an uploaded picture, else a demo drawing by category.
static std::string imageUrl(const MenuItem& m) {
    if (!m.imageFile.empty()) return "/images/" + m.imageFile;
    if (m.kind == ItemKind::Fresh) return "/img/demo/bowl.svg";
    if (m.category == "Chips") return "/img/demo/chips.svg";
    if (m.category == "Soft drinks") return "/img/demo/can.svg";
    return "/img/demo/bottle.svg";
}
#include <cstdlib>
#include <functional>

using httplib::Request;
using httplib::Response;

enum class Access { Public, User, Staff };  // User = student, teacher or university staff

struct Ctx {
    AppState& app;
    const Request& req;
    Response& res;
    Session* session;
    NotificationList out;
    bool changed;
};

static const int MAX_LIST = 2000;  // reservations returned in one response

static std::string cookieValue(const Request& req, const std::string& name) {
    std::string all = req.get_header_value("Cookie");
    std::string key = name + "=";
    size_t pos = 0;
    while (pos < all.size()) {
        while (pos < all.size() && (all[pos] == ' ' || all[pos] == ';')) pos++;
        size_t end = all.find(';', pos);
        if (end == std::string::npos) end = all.size();
        if (all.compare(pos, key.size(), key) == 0) return all.substr(pos + key.size(), end - pos - key.size());
        pos = end;
    }
    return "";
}

static void reply(Response& res, int status, const std::string& body) {
    res.status = status;
    res.set_content(body, "application/json; charset=utf-8");
}

static void fail(Ctx& c, int status, const std::string& message) { reply(c.res, status, jsonError(message)); }

static void handle(AppState& app, const Request& req, Response& res, Access access,
                   const std::function<void(Ctx&)>& fn) {
    // Only the picture upload may send a large body; every other route keeps 4 KB.
    if (req.body.size() > SMALL_BODY_LIMIT && req.path != IMAGE_UPLOAD_PATH) {
        reply(res, 413, jsonError("Request too large."));
        return;
    }
    if (req.method == "POST" && req.get_header_value("X-Cafe") != "1") {
        reply(res, 403, jsonError("Missing request header."));
        return;
    }
    Ctx c{app, req, res, nullptr, {}, false};
    nlInit(c.out);
    {
        std::lock_guard<std::mutex> lock(app.mu);
        cafEnsureToday(app.caf);  // menus and counters never show yesterday's numbers
        c.session = sessionGet(app.auth, cookieValue(req, "session"));
        bool allowed = true;
        if (access == Access::User || access == Access::Staff) {
            if (c.session == nullptr) {
                fail(c, 401, "Please log in.");
                allowed = false;
            } else if ((access == Access::Staff) != (c.session->role == Role::Staff)) {
                fail(c, 403, "You do not have access to this page.");
                allowed = false;
            }
        }
        if (allowed) fn(c);
        if (c.changed) saveAll(app);
    }
    sendAll(app, c.out);
}

static std::string param(const Request& req, const char* name) {
    return req.has_param(name) ? req.get_param_value(name) : "";
}

static int httpStatus(Result r) {
    switch (r) {
        case Result::Ok: return 200;
        case Result::BadItem:
        case Result::NotFresh:
        case Result::BadQuantity:
        case Result::InvalidValue:
        case Result::WrongKind: return 400;
        case Result::NotFound:
        case Result::NotOwner: return 404;
        default: return 409;
    }
}

static int httpStatus(AuthResult r) {
    switch (r) {
        case AuthResult::Ok: return 200;
        case AuthResult::BadCredentials: return 401;
        case AuthResult::Locked: return 423;
        case AuthResult::NotVerified: return 403;
        case AuthResult::AlreadyRegistered: return 409;
        case AuthResult::CryptoError: return 500;
        default: return 400;
    }
}

// "HH:MM" -> minute of day.
static bool parseClock(const std::string& s, int& minuteOfDay) {
    if (s.size() != 5 || s[2] != ':') return false;
    int h, m;
    if (!parseIntStrict(s.substr(0, 2), h) || !parseIntStrict(s.substr(3, 2), m)) return false;
    if (h < 0 || h > 23 || m < 0 || m > 59) return false;
    minuteOfDay = h * 60 + m;
    return true;
}

// "R-0042" or "42" -> 42.
static bool parseReservationCode(const std::string& s, int& id) {
    std::string digits = (s.size() > 2 && (s[0] == 'R' || s[0] == 'r') && s[1] == '-') ? s.substr(2) : s;
    return digits.size() <= 9 && parseIntStrict(digits, id) && id > 0;
}

static bool canCancel(const Cafeteria& caf, const Reservation* r) {
    return r->status == ResStatus::Reserved && r->day == dayKeyOf(caf.clock->now()) &&
           beforeMinuteOfDay(caf, caf.settings.cancelDeadlineMinute);
}

static void writeReservation(Json& j, const Cafeteria& caf, const Reservation* r, bool withUser) {
    const MenuItem* m = menuAt(caf.menu, r->itemIndex);
    j.beginObject();
    j.key("id").value(r->id);
    j.key("code").value(resCode(r->id));
    j.key("item").value(m ? m->name : "?");
    j.key("itemIndex").value(r->itemIndex);
    j.key("quantity").value(r->quantity);
    j.key("status").value(statusName(r->status));
    j.key("reservedAt").value(formatClock(r->reservedAt));
    j.key("day").value(r->day);
    j.key("readyAt").value(r->readyAt ? formatClock(r->readyAt) : "");
    j.key("freshUntil")
        .value(r->readyAt ? formatClock(r->expiresAt) : "");
    j.key("canCancel").value(canCancel(caf, r));
    if (withUser) j.key("user").value(r->user);
    j.endObject();
}

static void okMessage(Ctx& c, const std::string& message, bool undoRecorded) {
    Json j;
    j.beginObject().key("ok").value(true).key("message").value(message);
    j.key("undoRecorded").value(undoRecorded);
    if (!undoRecorded) j.key("warning").value("Undo history is full - this action cannot be undone.");
    j.endObject();
    reply(c.res, 200, j.str());
}

static std::string lastDescription(const Cafeteria& caf) {
    const StaffAction* a = stPeek(caf.history);
    return a ? a->description : "Done";
}

static void setSessionCookie(Ctx& c, const std::string& token, int maxAgeSeconds) {
    std::string cookie = "session=" + token + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=" +
                         std::to_string(maxAgeSeconds);
    if (c.app.secureCookies) cookie += "; Secure";
    c.res.set_header("Set-Cookie", cookie);
}

// ---- public: register, verify, login, logout ----------------------------------------

static void routeAuth(httplib::Server& svr, AppState& app) {
    svr.Get("/api/health", [&](const Request&, Response& res) { reply(res, 200, "{\"ok\":true}"); });

    svr.Post("/api/register", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Public, [](Ctx& c) {
            std::string email = param(c.req, "email"), id = param(c.req, "id"), pw = param(c.req, "password");
            if (email.size() > (size_t)MAX_EMAIL_LEN || id.size() > (size_t)MAX_UNI_ID_LEN ||
                pw.size() > (size_t)MAX_PASSWORD_LEN) {
                fail(c, 400, "Input too long.");
                return;
            }
            AuthResult r = registerStart(c.app.auth, email, id, pw, c.out);
            if (r != AuthResult::Ok) {
                fail(c, httpStatus(r), authMessage(r));
                return;
            }
            c.changed = true;
            reply(c.res, 200, "{\"ok\":true,\"message\":\"We sent a 6-digit code to your university email.\"}");
        });
    });

    svr.Post("/api/verify", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Public, [](Ctx& c) {
            AuthResult r =
                registerVerify(c.app.auth, param(c.req, "email"), param(c.req, "code"), param(c.req, "password"));
            c.changed = true;  // attempt counter changes on failure too
            if (r != AuthResult::Ok) {
                fail(c, httpStatus(r), authMessage(r));
                return;
            }
            reply(c.res, 200, "{\"ok\":true,\"message\":\"Email verified. You can log in now.\"}");
        });
    });

    auto doLogin = [&](const Request& req, Response& res, bool staffPortal) {
        handle(app, req, res, Access::Public, [staffPortal](Ctx& c) {
            std::string login = param(c.req, staffPortal ? "login" : "email");
            std::string token;
            AuthResult r = ::login(c.app.auth, login, param(c.req, "password"), staffPortal, token);
            c.changed = true;  // failed-login counters
            if (r != AuthResult::Ok) {
                fail(c, httpStatus(r), authMessage(r));
                return;
            }
            setSessionCookie(c, token, c.app.auth.cfg.sessionMinutes * 60);
            Session* s = sessionGet(c.app.auth, token);
            Json j;
            j.beginObject().key("ok").value(true).key("role").value(roleName(s->role)).endObject();
            reply(c.res, 200, j.str());
        });
    };
    svr.Post("/api/login", [doLogin](const Request& req, Response& res) { doLogin(req, res, false); });
    svr.Post("/api/staff/login", [doLogin](const Request& req, Response& res) { doLogin(req, res, true); });

    svr.Post("/api/logout", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Public, [](Ctx& c) {
            if (c.session) logout(c.app.auth, c.session->token);
            setSessionCookie(c, "", 0);
            reply(c.res, 200, "{\"ok\":true}");
        });
    });

    svr.Get("/api/me", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Public, [](Ctx& c) {
            if (!c.session) {
                fail(c, 401, "Please log in.");
                return;
            }
            User* u = ulFind(c.app.auth.users, c.session->login);
            Json j;
            j.beginObject().key("ok").value(true);
            j.key("login").value(c.session->login);
            j.key("role").value(roleName(c.session->role));
            j.key("name").value(u ? u->name : "");
            j.key("universityId").value(u ? u->universityId : "");
            j.key("department").value(u ? u->department : "");
            j.endObject();
            reply(c.res, 200, j.str());
        });
    });
}

// ---- students and teachers -----------------------------------------------------------

static void routeUser(httplib::Server& svr, AppState& app) {
    svr.Get("/api/menu", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::User, [](Ctx& c) {
            const Cafeteria& caf = c.app.caf;
            Json j;
            j.beginObject().key("ok").value(true);
            j.key("now").value(formatClock(caf.clock->now()));
            j.key("cutoff").value(formatMinuteOfDay(caf.settings.cutoffMinute));
            j.key("cancelDeadline").value(formatMinuteOfDay(caf.settings.cancelDeadlineMinute));
            j.key("open").value(beforeMinuteOfDay(caf, caf.settings.cutoffMinute));
            j.key("perUserLimit").value(caf.settings.perUserLimit);
            j.key("freshness").value(durationText(caf.settings.freshnessMinutes));
            j.key("items").beginArray();
            for (int i = 0; i < caf.menu.count; i++) {
                const MenuItem& m = caf.menu.items[i];
                if (!m.available) continue;  // hidden while staff have it switched off
                j.beginObject().key("index").value(i).key("name").value(m.name);
                j.key("category").value(m.category);
                j.key("price").value(m.priceTaka);
                j.key("image").value(imageUrl(m));
                if (m.kind == ItemKind::Fresh) {
                    j.key("kind").value("fresh");
                    j.key("remaining").value(menuRemaining(m));
                    j.key("limit").value(m.dailyLimit);
                    j.key("yours").value(userPortionsToday(caf, c.session->login, i));
                    j.key("waiting").value(waitlistActiveCount(caf, i));
                    j.key("yourPosition").value(waitlistPosition(caf, c.session->login, i));
                } else {
                    j.key("kind").value("packaged");
                    j.key("stock").value(m.stock);
                }
                j.endObject();
            }
            j.endArray().endObject();
            reply(c.res, 200, j.str());
        });
    });

    svr.Post("/api/reservations", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::User, [](Ctx& c) {
            int item, qty;
            if (!parseIntStrict(param(c.req, "item"), item)) {
                fail(c, 400, resultMessage(Result::BadItem));
                return;
            }
            if (!parseIntStrict(param(c.req, "quantity"), qty)) {
                fail(c, 400, resultMessage(Result::BadQuantity));
                return;
            }
            Reservation* r = nullptr;
            Result result = reserve(c.app.caf, c.session->login, item, qty, c.out, &r);
            if (result != Result::Ok) {
                fail(c, httpStatus(result), resultMessage(result));
                return;
            }
            c.changed = true;
            Json j;
            j.beginObject().key("ok").value(true).key("id").value(r->id).key("code").value(resCode(r->id));
            j.key("message").value("Reservation " + resCode(r->id) + " confirmed. Confirmation email sent.");
            j.endObject();
            reply(c.res, 200, j.str());
        });
    });

    svr.Get("/api/reservations/mine", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::User, [](Ctx& c) {
            Reservation** arr = new Reservation*[MAX_LIST];
            int n = listUserReservations(c.app.caf, c.session->login, arr, MAX_LIST);
            Json j;
            j.beginObject().key("ok").value(true).key("reservations").beginArray();
            for (int i = n - 1; i >= 0; i--) writeReservation(j, c.app.caf, arr[i], false);  // newest first
            j.endArray().endObject();
            delete[] arr;
            reply(c.res, 200, j.str());
        });
    });

    svr.Post("/api/waitlist/join", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::User, [](Ctx& c) {
            int item, qty, position = 0;
            if (!parseIntStrict(param(c.req, "item"), item)) {
                fail(c, 400, resultMessage(Result::BadItem));
                return;
            }
            if (!parseIntStrict(param(c.req, "quantity"), qty)) {
                fail(c, 400, resultMessage(Result::BadQuantity));
                return;
            }
            Result r = joinWaitlist(c.app.caf, c.session->login, item, qty, position);
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            Json j;
            j.beginObject().key("ok").value(true).key("position").value(position);
            j.key("message").value("You are #" + std::to_string(position) + " on the " +
                                   c.app.caf.menu.items[item].name + " waitlist. If a portion becomes free before " +
                                   formatMinuteOfDay(c.app.caf.settings.cutoffMinute) +
                                   ", it is reserved for you and we email you.");
            j.endObject();
            reply(c.res, 200, j.str());
        });
    });

    svr.Post("/api/waitlist/leave", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::User, [](Ctx& c) {
            int item;
            if (!parseIntStrict(param(c.req, "item"), item)) {
                fail(c, 400, resultMessage(Result::BadItem));
                return;
            }
            Result r = leaveWaitlist(c.app.caf, c.session->login, item);
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            reply(c.res, 200, "{\"ok\":true,\"message\":\"You left the waitlist.\"}");
        });
    });

    svr.Post("/api/reservations/cancel", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::User, [](Ctx& c) {
            int id;
            if (!parseReservationCode(param(c.req, "id"), id)) {
                fail(c, 404, resultMessage(Result::NotFound));
                return;
            }
            Result r = cancelReservation(c.app.caf, c.session->login, id, c.out);
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            reply(c.res, 200, "{\"ok\":true,\"message\":\"Reservation " + resCode(id) + " cancelled.\"}");
        });
    });
}

// ---- staff portal ------------------------------------------------------------------------

static void routeStaff(httplib::Server& svr, AppState& app) {
    svr.Get("/api/staff/overview", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            const Cafeteria& caf = c.app.caf;
            Json j;
            j.beginObject().key("ok").value(true);
            j.key("now").value(formatClock(caf.clock->now()));
            j.key("staffName").value(c.session->login);
            j.key("emailMode").value(c.app.email->modeName());
            j.key("settings").beginObject();
            j.key("cutoff").value(formatMinuteOfDay(caf.settings.cutoffMinute));
            j.key("cancel_deadline").value(formatMinuteOfDay(caf.settings.cancelDeadlineMinute));
            j.key("freshness_minutes").value(caf.settings.freshnessMinutes);
            j.key("reminder_minutes").value(caf.settings.reminderIntervalMinutes);
            j.key("per_user_limit").value(caf.settings.perUserLimit);
            j.endObject();
            j.key("items").beginArray();
            for (int i = 0; i < caf.menu.count; i++) {
                const MenuItem& m = caf.menu.items[i];
                j.beginObject().key("index").value(i).key("name").value(m.name).key("category").value(m.category);
                j.key("kind").value(m.kind == ItemKind::Fresh ? "fresh" : "packaged");
                j.key("stock").value(m.stock);
                j.key("price").value(m.priceTaka);
                j.key("image").value(imageUrl(m));
                j.key("available").value(m.available);
                if (m.kind == ItemKind::Fresh) {
                    int waiting = 0;
                    for (int k = 0; k < caf.cooking[i].count; k++)
                        if (qAt(caf.cooking[i], k)->status == ResStatus::Reserved)
                            waiting += qAt(caf.cooking[i], k)->quantity;
                    j.key("limit").value(m.dailyLimit).key("reserved").value(m.reservedToday);
                    j.key("remaining").value(menuRemaining(m));
                    j.key("waitingPortions").value(waiting);
                    j.key("queueEntries").value(caf.cooking[i].count);
                    j.key("queueCapacity").value(caf.cooking[i].capacity);
                    j.key("waitlist").value(waitlistActiveCount(caf, i));
                }
                j.endObject();
            }
            j.endArray();
            j.key("reminderQueue").value(caf.reminders.count);
            j.key("emailsWaiting").value(mailerWaiting(c.app.mailer));  // app.mu -> mailer.mu (allowed order)
            j.key("historySize").value(stSize(caf.history));
            j.key("historyCapacity").value(ACTION_STACK_CAPACITY);
            j.key("history").beginArray();
            for (int i = 0; i < stSize(caf.history); i++) {
                const StaffAction* a = stFromTop(caf.history, i);
                j.beginObject().key("description").value(a->description).key("staff").value(a->staff);
                j.key("at").value(formatClock(a->at)).endObject();
            }
            j.endArray().endObject();
            reply(c.res, 200, j.str());
        });
    });

    svr.Get("/api/staff/reservations", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            std::string q = param(c.req, "q");
            Json j;
            j.beginObject().key("ok").value(true).key("reservations").beginArray();
            if (!q.empty()) {
                int id;
                if (!parseReservationCode(q, id)) {
                    fail(c, 400, "Search by reservation ID, for example R-0042.");
                    return;
                }
                Reservation* r = staffSearchById(c.app.caf, id);
                if (r) writeReservation(j, c.app.caf, r, true);
            } else {
                Reservation** arr = new Reservation*[MAX_LIST];
                int n = staffReservationsSorted(c.app.caf, arr, MAX_LIST);
                for (int i = 0; i < n; i++) writeReservation(j, c.app.caf, arr[i], true);
                delete[] arr;
            }
            j.endArray().endObject();
            reply(c.res, 200, j.str());
        });
    });

    svr.Post("/api/staff/stock", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            int item, value;
            bool rec = false;
            if (!parseIntStrict(param(c.req, "item"), item) || !parseIntStrict(param(c.req, "value"), value)) {
                fail(c, 400, "Enter a whole number.");
                return;
            }
            Result r = setStock(c.app.caf, c.session->login, item, value, rec);
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            okMessage(c, lastDescription(c.app.caf), rec);
        });
    });

    svr.Post("/api/staff/limit", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            int item, value;
            bool rec = false;
            if (!parseIntStrict(param(c.req, "item"), item) || !parseIntStrict(param(c.req, "value"), value)) {
                fail(c, 400, "Enter a whole number.");
                return;
            }
            Result r = setDailyLimit(c.app.caf, c.session->login, item, value, c.out, rec);
            if (r == Result::InvalidValue) {
                const MenuItem* m = menuAt(c.app.caf.menu, item);
                fail(c, 400, "The limit must be between portions already reserved (" +
                                 std::to_string(m ? m->reservedToday : 0) + ") and " +
                                 std::to_string(c.app.caf.cookingCapacity) + ".");
                return;
            }
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            okMessage(c, lastDescription(c.app.caf), rec);
        });
    });

    svr.Post("/api/staff/ready", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            int item, portions = 0;
            std::string p = param(c.req, "portions");
            if (!parseIntStrict(param(c.req, "item"), item) || (!p.empty() && !parseIntStrict(p, portions))) {
                fail(c, 400, "Enter a whole number.");
                return;
            }
            ReadyReport rep;
            Result r = markReady(c.app.caf, c.session->login, item, portions, c.out, rep);
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            Json j;
            j.beginObject().key("ok").value(true).key("madeReady").value(rep.madeReady);
            j.key("skipped").value(rep.skipped).key("leftWaiting").value(rep.leftWaiting);
            j.key("message").value(std::to_string(rep.madeReady) + " reservation(s) marked ready and emailed; " +
                                   std::to_string(rep.skipped) + " cancelled skipped; " +
                                   std::to_string(rep.leftWaiting) + " still waiting.");
            j.endObject();
            reply(c.res, 200, j.str());
        });
    });

    svr.Post("/api/staff/collected", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            int id;
            bool rec = false;
            if (!parseReservationCode(param(c.req, "id"), id)) {
                fail(c, 404, resultMessage(Result::NotFound));
                return;
            }
            Result r = markCollected(c.app.caf, c.session->login, id, rec);
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            okMessage(c, lastDescription(c.app.caf), rec);
        });
    });

    svr.Post("/api/staff/undo", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            StaffAction undone;
            Result r = undoLast(c.app.caf, c.session->login, c.out, undone);
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            reply(c.res, 200, "{\"ok\":true,\"message\":\"Undid: " + jsonEscape(undone.description) + "\"}");
        });
    });

    svr.Post("/api/staff/settings", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            std::string key = param(c.req, "key"), raw = param(c.req, "value");
            int value;
            bool timeKey = key == "cutoff" || key == "cancel_deadline";
            bool parsed = timeKey ? parseClock(raw, value) : parseIntStrict(raw, value);
            bool rec = false;
            Result r = parsed ? changeSetting(c.app.caf, c.session->login, key, value, rec) : Result::InvalidValue;
            if (r != Result::Ok) {
                fail(c, 400, timeKey ? "Enter a time as HH:MM (24-hour)." : "That value is out of range.");
                return;
            }
            c.changed = true;
            okMessage(c, lastDescription(c.app.caf), rec);
        });
    });
}

// ---- testing / demo: move the fake clock ------------------------------------------------

// ---- redesign: public info, tray, staff-managed items -------------------------------

// "3:1,2:2" -> lines. False on anything else (bad numbers, too many lines).
static bool parseTray(const std::string& s, TrayLine* lines, int& n) {
    n = 0;
    if (s.empty() || s.size() > 512) return false;
    std::string parts[MAX_MENU_ITEMS];
    int count = splitFields(s, ',', parts, MAX_MENU_ITEMS);
    if (count < 1) return false;
    for (int i = 0; i < count; i++) {
        std::string f[2];
        if (splitFields(parts[i], ':', f, 2) != 2 || !parseIntStrict(f[0], lines[i].itemIndex) ||
            !parseIntStrict(f[1], lines[i].quantity))
            return false;
    }
    n = count;
    return true;
}

static void routeRedesign(httplib::Server& svr, AppState& app) {
    svr.Get("/api/public/info", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Public, [](Ctx& c) {
            const Cafeteria& caf = c.app.caf;
            Json j;
            j.beginObject().key("ok").value(true);
            j.key("now").value(formatClock(caf.clock->now()));
            j.key("cutoff").value(formatMinuteOfDay(caf.settings.cutoffMinute));
            j.key("cancelDeadline").value(formatMinuteOfDay(caf.settings.cancelDeadlineMinute));
            j.key("freshness").value(durationText(caf.settings.freshnessMinutes));
            j.key("open").value(beforeMinuteOfDay(caf, caf.settings.cutoffMinute));  // the announcement bar
            j.endObject();
            reply(c.res, 200, j.str());
        });
    });

    svr.Post("/api/reservations/batch", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::User, [](Ctx& c) {
            TrayLine lines[MAX_MENU_ITEMS];
            int n = 0;
            if (!parseTray(param(c.req, "items"), lines, n)) {
                fail(c, 400, "Your tray could not be read. Reload the page and try again.");
                return;
            }
            int failed = -1;
            int ids[MAX_MENU_ITEMS];
            Result r = reserveBatch(c.app.caf, c.session->login, lines, n, c.out, failed, ids);
            if (r != Result::Ok) {
                const MenuItem* m = failed >= 0 ? menuAt(c.app.caf.menu, lines[failed].itemIndex) : nullptr;
                fail(c, httpStatus(r), (m ? m->name + ": " : std::string()) + resultMessage(r));
                return;
            }
            c.changed = true;
            int total = 0;
            Json j;
            j.beginObject().key("ok").value(true).key("codes").beginArray();
            for (int i = 0; i < n; i++) {
                j.value(resCode(ids[i]));
                total += c.app.caf.menu.items[lines[i].itemIndex].priceTaka * lines[i].quantity;
            }
            j.endArray().key("total").value(total);
            j.key("message").value("Reserved. Confirmation email sent. Pay at the counter when you collect.");
            j.endObject();
            reply(c.res, 200, j.str());
        });
    });

    // Order history: earlier days' reservations from history.txt, newest first
    // (loaded into a linked list and merge-sorted), plus nothing from today
    // (today's are on /api/reservations/mine).
    svr.Get("/api/reservations/history", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::User, [](Ctx& c) {
            ReservationList past;
            rlInit(past);
            std::string err;
            loadUserHistory(c.app.dataDir, c.session->login, past, err);
            if (!err.empty()) std::fprintf(stderr, "HISTORY: %s\n", err.c_str());
            Json j;
            j.beginObject().key("ok").value(true).key("count").value(past.size).key("orders").beginArray();
            for (Reservation* r = past.head; r != nullptr; r = r->next) {
                const MenuItem* m = menuAt(c.app.caf.menu, r->itemIndex);
                j.beginObject().key("code").value(resCode(r->id)).key("item").value(m ? m->name : "?");
                j.key("quantity").value(r->quantity).key("status").value(statusName(r->status));
                j.key("day").value(r->day).key("reservedAt").value(formatClock(r->reservedAt)).endObject();
            }
            j.endArray().endObject();
            rlClear(past);
            reply(c.res, 200, j.str());
        });
    });

    svr.Post("/api/account/password", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::User, [](Ctx& c) {
            AuthResult r = changePassword(c.app.auth, c.session->login, param(c.req, "current"),
                                          param(c.req, "password"), c.session->token);
            c.changed = true;  // failed-attempt counter or new hash
            if (r == AuthResult::BadCredentials) {
                fail(c, 401, "Your current password is not correct.");
                return;
            }
            if (r == AuthResult::WeakPassword) {
                fail(c, 400, "Choose a new password of 8 to 128 characters, different from the current one.");
                return;
            }
            if (r != AuthResult::Ok) {
                fail(c, httpStatus(r), authMessage(r));
                return;
            }
            reply(c.res, 200,
                  "{\"ok\":true,\"message\":\"Password changed. You were signed out on your other devices.\"}");
        });
    });

    svr.Post("/api/staff/items", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            std::string kind = param(c.req, "kind");
            int price, amount, index = -1;
            if ((kind != "fresh" && kind != "packaged") || !parseIntStrict(param(c.req, "price"), price) ||
                !parseIntStrict(param(c.req, "amount"), amount)) {
                fail(c, 400, "Fill in every field; price and amount are whole numbers.");
                return;
            }
            Result r = addMenuItem(c.app.caf, c.session->login, param(c.req, "category"), param(c.req, "name"),
                                   kind == "fresh" ? ItemKind::Fresh : ItemKind::Packaged, price, amount, index);
            if (r != Result::Ok) {
                fail(c, httpStatus(r),
                     r == Result::InvalidValue
                         ? "Check the item: a new name (1-60 characters, no |), price 0-9999, a sensible amount."
                         : resultMessage(r));
                return;
            }
            c.changed = true;
            Json j;
            j.beginObject().key("ok").value(true).key("index").value(index);
            j.key("message").value("Added " + c.app.caf.menu.items[index].name + ".").endObject();
            reply(c.res, 200, j.str());
        });
    });

    svr.Post("/api/staff/items/price", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            int item, price;
            bool rec = false;
            if (!parseIntStrict(param(c.req, "item"), item) || !parseIntStrict(param(c.req, "price"), price)) {
                fail(c, 400, "Enter a whole number.");
                return;
            }
            Result r = setPrice(c.app.caf, c.session->login, item, price, rec);
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            okMessage(c, lastDescription(c.app.caf), rec);
        });
    });

    svr.Post("/api/staff/items/available", [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            int item;
            std::string a = param(c.req, "available");
            bool rec = false;
            if (!parseIntStrict(param(c.req, "item"), item) || (a != "0" && a != "1")) {
                fail(c, 400, "Choose available or not available.");
                return;
            }
            Result r = setAvailable(c.app.caf, c.session->login, item, a == "1", c.out, rec);
            if (r != Result::Ok) {
                fail(c, httpStatus(r), resultMessage(r));
                return;
            }
            c.changed = true;
            okMessage(c, lastDescription(c.app.caf), rec);
        });
    });

    svr.Post(IMAGE_UPLOAD_PATH, [&](const Request& req, Response& res) {
        handle(app, req, res, Access::Staff, [](Ctx& c) {
            int item;
            if (!c.req.is_multipart_form_data() || !c.req.form.has_file("image") ||
                !parseIntStrict(c.req.form.get_field("item"), item)) {
                fail(c, 400, "Choose an item and a picture.");
                return;
            }
            MenuItem* m = menuAt(c.app.caf.menu, item);
            if (m == nullptr) {
                fail(c, 400, resultMessage(Result::BadItem));
                return;
            }
            std::string newFile, err;
            if (!storeItemImage(c.app.dataDir, item, c.req.form.get_file("image").content, m->imageFile, newFile,
                                err)) {
                fail(c, 400, err);
                return;
            }
            setItemImage(c.app.caf, c.session->login, item, newFile);
            c.changed = true;
            Json j;
            j.beginObject().key("ok").value(true).key("image").value("/images/" + newFile);
            j.key("message").value("Picture updated for " + m->name + ".").endObject();
            reply(c.res, 200, j.str());
        });
    });
}

static void routeDev(httplib::Server& svr, AppState& app) {
    svr.Post("/api/dev/advance", [&](const Request& req, Response& res) {
        if (req.remote_addr != "127.0.0.1" && req.remote_addr != "::1") {
            reply(res, 403, jsonError("Local only."));
            return;
        }
        handle(app, req, res, Access::Public, [](Ctx& c) {
            int minutes;
            if (!parseIntStrict(param(c.req, "minutes"), minutes) || minutes < 1 || minutes > 7 * 24 * 60) {
                fail(c, 400, "minutes must be 1..10080");
                return;
            }
            for (int i = 0; i < minutes; i++) {
                c.app.fakeClock->advanceMinutes(1);
                timerTick(c.app.caf, c.out);
            }
            c.changed = true;
            reply(c.res, 200, "{\"ok\":true,\"now\":\"" + formatClock(c.app.clock->now()) + "\"}");
        });
        mailerRetryDue(app.mailer);  // what the timer thread does each minute; outside app.mu
    });
}

// ---- page guards and headers -----------------------------------------------------------

static bool pageAllowed(AppState& app, const Request& req, bool staffPage) {
    std::lock_guard<std::mutex> lock(app.mu);
    Session* s = sessionGet(app.auth, cookieValue(req, "session"));
    return s != nullptr && (s->role == Role::Staff) == staffPage;
}

void registerRoutes(httplib::Server& svr, AppState& app, bool devRoutes) {
    svr.set_payload_max_length(MAX_IMAGE_BYTES + 64 * 1024);  // handle() keeps other routes at 4 KB

    // Pages that need a login: send people to the right login page instead.
    svr.set_pre_routing_handler([&](const Request& req, Response& res) {
        // First: outside the cafeteria's own computers the staff terminal does not
        // exist - the same empty 404 httplib gives for a missing page.
        if (isStaffPath(req.path) && !staffComputerAllowed(app.staffComputers, req.remote_addr)) {
            res.status = 404;
            if (req.has_header("Content-Length") || req.has_header("Transfer-Encoding"))
                res.set_header("Connection", "close");  // the body is left unread
            return httplib::Server::HandlerResponse::Handled;
        }
        bool staffPage = req.path == "/staff/portal.html";
        if (req.path == "/home.html") {  // old address of the menu page
            res.set_redirect("/menu.html");
            return httplib::Server::HandlerResponse::Handled;
        }
        // Runs before httplib reads the body. Multipart bodies go to req.form, not
        // req.body, so handle()'s size check alone would let them through.
        if (req.path != IMAGE_UPLOAD_PATH &&
            (std::strtoull(req.get_header_value("Content-Length").c_str(), nullptr, 10) > SMALL_BODY_LIMIT ||
             req.has_header("Transfer-Encoding"))) {
            reply(res, 413, jsonError("Request too large."));
            res.set_header("Connection", "close");  // the unread body must not be parsed as a request
            return httplib::Server::HandlerResponse::Handled;
        }
        bool userPage = req.path == "/menu.html" || req.path == "/reservations.html" || req.path == "/account.html";
        if ((staffPage || userPage) && !pageAllowed(app, req, staffPage)) {
            res.set_redirect(staffPage ? "/staff/login.html" : "/");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.set_post_routing_handler([](const Request& req, Response& res) {
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_header("X-Frame-Options", "DENY");
        res.set_header("Referrer-Policy", "no-referrer");
        res.set_header("Content-Security-Policy",
                       "default-src 'self'; img-src 'self' data:; frame-ancestors 'none'; form-action 'self'");
        if (req.path.rfind("/api/", 0) == 0) res.set_header("Cache-Control", "no-store");
    });

    routeAuth(svr, app);
    routeUser(svr, app);
    routeStaff(svr, app);
    routeRedesign(svr, app);
    if (devRoutes) routeDev(svr, app);
}
