// Online demo backend (GitHub Pages only). The real system is the C++ server in
// src/; this file answers the same /api/... calls inside the browser, with fake
// data kept in this browser's localStorage, so the pages can be tried without a
// server. Nothing is sent anywhere. It follows the server's main rules (cutoff,
// per-person limit, all-or-nothing tray, FIFO "ready", waitlist, undo) but is a
// demo, not the system under test.
"use strict";

(() => {
  const BASE = new URL(".", document.currentScript.src).pathname;  // e.g. /cafeteria-pre-order-system/
  const KEY = "cafe-demo-v1";
  const SESSION = "cafe-demo-session";
  const TODAY = 20261024;
  const NOW = 8 * 60 + 30;  // the demo clock stays at 08:30, before the 10:00 cutoff
  const img = (f) => BASE + "img/demo/" + f;

  const ACCOUNTS = [
    { login: "ayon@uni.example", password: "demo-student-pass", role: "student", name: "Ayon Das", universityId: "9990000000000101", department: "CSE" },
    { login: "teacher1@uni.example", password: "demo-teacher-pass", role: "teacher", name: "Sample Teacher", universityId: "2000000000000001", department: "CSE" },
    { login: "guard1@uni.example", password: "demo-guard-pass", role: "employee", name: "Sample Guard", universityId: "3000000000000002", department: "Security" },
    { login: "demo.staff", password: "demo-staff-pass", role: "staff", name: "Demo Staff", universityId: "", department: "" },
  ];
  // Rows of the (fake) university list that can register in the demo.
  const UNI_LIST = [
    { id: "9990000000000102", email: "ali@uni.example", role: "student", name: "Mohammad Ali Gazi", department: "CSE" },
    { id: "3000000000000001", email: "", role: "employee", name: "Rahim Driver", department: "Transport" },
  ];

  function fresh(name, price, limit) {
    return { name, category: "Fresh food", kind: "fresh", price, limit, stock: 0, image: img("bowl.svg"), available: true };
  }
  function packaged(category, name, price, stock, file) {
    return { name, category, kind: "packaged", price, limit: 0, stock, image: img(file), available: true };
  }
  function seed() {
    return {
      nextId: 7,
      settings: { cutoff: 600, cancel_deadline: 600, freshness_minutes: 420, reminder_minutes: 60, per_user_limit: 2 },
      items: [
        fresh("Khichuri", 80, 12), fresh("Fried Rice", 100, 8), fresh("Tehari", 120, 10),
        packaged("Chips", "Alloz", 20, 18, "chips.svg"), packaged("Chips", "Lays", 30, 24, "chips.svg"),
        packaged("Chips", "Sun", 20, 0, "chips.svg"), packaged("Soft drinks", "Dew", 40, 12, "can.svg"),
        packaged("Soft drinks", "Pepsi", 40, 20, "can.svg"), packaged("Soft drinks", "Coca-Cola", 40, 0, "can.svg"),
        packaged("Water", "Drinking water", 20, 30, "bottle.svg"),
      ],
      users: ACCOUNTS.map((a) => Object.assign({}, a)),
      pending: [],
      reservations: [
        // two other people already reserved today
        { id: 5, user: "other1@uni.example", item: 2, quantity: 2, status: "Reserved", at: "08:05", day: TODAY, readyAt: "" },
        { id: 6, user: "other2@uni.example", item: 1, quantity: 1, status: "Reserved", at: "08:12", day: TODAY, readyAt: "" },
        // earlier days, for "Order history"
        { id: 1, user: "ayon@uni.example", item: 2, quantity: 1, status: "Collected", at: "09:10", day: 20261022, readyAt: "12:30" },
        { id: 2, user: "ayon@uni.example", item: 0, quantity: 2, status: "Expired", at: "08:40", day: 20261023, readyAt: "12:00" },
        { id: 3, user: "guard1@uni.example", item: 1, quantity: 1, status: "Collected", at: "09:30", day: 20261023, readyAt: "12:15" },
        { id: 4, user: "teacher1@uni.example", item: 0, quantity: 1, status: "Cancelled", at: "08:55", day: 20261023, readyAt: "" },
      ],
      waitlist: {},  // item index -> [{ user, quantity }]
      undo: [],      // newest last; at most 50 (the server's array stack)
    };
  }

  let db;
  try {
    db = JSON.parse(localStorage.getItem(KEY)) || seed();
  } catch (e) {
    db = seed();
  }
  const save = () => {
    try {
      localStorage.setItem(KEY, JSON.stringify(db));
    } catch (e) {
      /* storage blocked: the demo lasts until the page is closed */
    }
  };
  const session = {
    get() {
      try {
        return JSON.parse(sessionStorage.getItem(SESSION));
      } catch (e) {
        return null;
      }
    },
    set(v) {
      try {
        if (v) sessionStorage.setItem(SESSION, JSON.stringify(v));
        else sessionStorage.removeItem(SESSION);
      } catch (e) {
        /* ignore */
      }
    },
  };

  const hhmm = (m) => String(Math.floor(m / 60)).padStart(2, "0") + ":" + String(m % 60).padStart(2, "0");
  const code = (id) => "R-" + String(id).padStart(4, "0");
  const open = () => NOW < db.settings.cutoff;
  const today = (r) => r.day === TODAY;
  const live = (r) => today(r) && r.status !== "Cancelled";
  const reserved = (i) => db.reservations.filter((r) => live(r) && r.item === i).reduce((s, r) => s + r.quantity, 0);
  const remaining = (i) => Math.max(0, db.items[i].limit - reserved(i));
  const yours = (u, i) => db.reservations.filter((r) => live(r) && r.user === u && r.item === i).reduce((s, r) => s + r.quantity, 0);
  const wl = (i) => db.waitlist[i] || (db.waitlist[i] = []);
  const reply = (status, body) => ({ status, body: Object.assign({ ok: status === 200 }, body) });
  const ok = (body) => reply(200, body || {});
  const fail = (status, error) => reply(status, { error });

  function who() {
    const s = session.get();
    return s ? db.users.find((u) => u.login === s.login) || null : null;
  }
  function resJson(r, withUser) {
    const fresh = r.readyAt ? hhmm(Math.min(24 * 60 - 1, toMin(r.readyAt) + db.settings.freshness_minutes)) : "";
    const out = {
      id: r.id, code: code(r.id), item: db.items[r.item] ? db.items[r.item].name : "?", itemIndex: r.item,
      quantity: r.quantity, status: r.status, reservedAt: r.at, day: r.day, readyAt: r.readyAt, freshUntil: fresh,
      canCancel: r.status === "Reserved" && NOW < db.settings.cancel_deadline,
    };
    if (withUser) out.user = r.user;
    return out;
  }
  function toMin(t) {
    const [h, m] = t.split(":").map(Number);
    return h * 60 + m;
  }
  function book(user, i, q) {
    const r = { id: db.nextId++, user, item: i, quantity: q, status: "Reserved", at: hhmm(NOW), day: TODAY, readyAt: "" };
    db.reservations.push(r);
    db.waitlist[i] = wl(i).filter((w) => w.user !== user);
    return r;
  }
  function checkLine(user, i, q, claimed) {
    const it = db.items[i];
    if (!it) return "That dish does not exist.";
    if (it.kind !== "fresh") return it.name + ": packaged items are bought at the counter, not reserved.";
    if (!it.available) return it.name + ": not available today.";
    if (!open()) return "Reservations closed at " + hhmm(db.settings.cutoff) + ".";
    if (!(q >= 1 && q <= db.settings.per_user_limit)) return it.name + ": choose 1 to " + db.settings.per_user_limit + " portions.";
    if (q + claimed > remaining(i)) return it.name + ": Not enough portions left for that quantity.";
    if (yours(user, i) + claimed + q > db.settings.per_user_limit)
      return it.name + ": You already have the most portions allowed per person.";
    return "";
  }
  // A freed portion goes to the first person waiting who can take it (FIFO).
  function promote(i) {
    const q = wl(i);
    for (let n = 0; n < q.length && remaining(i) > 0; ) {
      const w = q[n];
      if (w.quantity <= remaining(i) && yours(w.user, i) + w.quantity <= db.settings.per_user_limit) {
        q.splice(n, 1);
        book(w.user, i, w.quantity);
      } else n++;
    }
  }
  function pushUndo(staff, description, revert) {
    db.undo.push({ description, at: hhmm(NOW), staff, revert });
    if (db.undo.length > 50) db.undo.shift();
  }
  function intIn(v, lo, hi) {
    if (!/^\d+$/.test(String(v))) return null;
    const n = Number(v);
    return n >= lo && n <= hi ? n : null;
  }

  const routes = {
    "GET /api/health": () => ok(),
    "GET /api/public/info": () => ok({
      now: hhmm(NOW), cutoff: hhmm(db.settings.cutoff), cancelDeadline: hhmm(db.settings.cancel_deadline),
      freshness: db.settings.freshness_minutes / 60 + " hours", open: open(),
    }),
    "GET /api/me": () => {
      const u = who();
      if (!u) return fail(401, "Please log in.");
      return ok({ login: u.login, role: u.role, name: u.name, universityId: u.universityId, department: u.department });
    },
    "POST /api/login": (p) => {
      const u = db.users.find((x) => x.login === String(p.email || "").trim().toLowerCase());
      if (!u || u.role === "staff" || u.password !== p.password) return fail(401, "Wrong login or password.");
      session.set({ login: u.login });
      return ok({ role: u.role });
    },
    "POST /api/staff/login": (p) => {
      const u = db.users.find((x) => x.login === String(p.login || "").trim().toLowerCase());
      if (!u || u.role !== "staff" || u.password !== p.password) return fail(401, "Wrong login or password.");
      session.set({ login: u.login });
      return ok({ role: "staff" });
    },
    "POST /api/logout": () => {
      session.set(null);
      return ok();
    },
    "POST /api/register": (p) => {
      const email = String(p.email || "").trim().toLowerCase();
      const row = UNI_LIST.find((r) => r.id === String(p.id || "").trim()) ||
        ACCOUNTS.filter((a) => a.universityId).map((a) => ({ id: a.universityId, email: a.login })).find((r) => r.id === String(p.id || "").trim());
      if (row && !row.email) return fail(400, "You have no university email. Ask the cafeteria office to create your account.");
      if (!email.endsWith("@uni.example")) return fail(400, "Please use your university email address.");
      if (!row) return fail(400, "That ID is not on the university's list.");
      if (row.email !== email) return fail(400, "That email does not match the university record for this ID.");
      if (db.users.some((u) => u.login === email)) return fail(409, "An account already exists for this email.");
      if (String(p.password || "").length < 8) return fail(400, "Password must be 8 to 128 characters.");
      const c = String(Math.floor(100000 + Math.random() * 900000));
      db.pending = db.pending.filter((x) => x.email !== email).concat([{ email, code: c, password: p.password, row }]);
      save();
      return ok({ message: "Demo: no email is sent. Your 6-digit code is " + c + "." });
    },
    "POST /api/verify": (p) => {
      const email = String(p.email || "").trim().toLowerCase();
      const pend = db.pending.find((x) => x.email === email);
      if (!pend || pend.code !== String(p.code || "").trim() || pend.password !== p.password)
        return fail(400, "That code is not correct, or the password differs from the one you chose.");
      db.users.push({ login: email, password: pend.password, role: pend.row.role, name: pend.row.name, universityId: pend.row.id, department: pend.row.department });
      db.pending = db.pending.filter((x) => x !== pend);
      save();
      return ok({ message: "Email verified. You can log in now." });
    },
    "GET /api/menu": () => {
      const u = who();
      if (!u) return fail(401, "Please log in.");
      if (u.role === "staff") return fail(403, "You do not have access to this page.");
      const items = db.items.map((it, index) => ({ it, index })).filter(({ it }) => it.available).map(({ it, index }) =>
        it.kind === "fresh"
          ? { index, name: it.name, category: it.category, price: it.price, image: it.image, kind: "fresh", remaining: remaining(index),
              limit: it.limit, yours: yours(u.login, index), waiting: wl(index).length,
              yourPosition: wl(index).findIndex((w) => w.user === u.login) + 1 }
          : { index, name: it.name, category: it.category, price: it.price, image: it.image, kind: "packaged", stock: it.stock });
      return ok({ now: hhmm(NOW), cutoff: hhmm(db.settings.cutoff), cancelDeadline: hhmm(db.settings.cancel_deadline), open: open(),
        perUserLimit: db.settings.per_user_limit, freshness: db.settings.freshness_minutes / 60 + " hours", items });
    },
    "POST /api/reservations/batch": (p) => {
      const u = who();
      if (!u || u.role === "staff") return fail(u ? 403 : 401, u ? "You do not have access to this page." : "Please log in.");
      const lines = String(p.items || "").split(",").filter(Boolean).map((s) => s.split(":").map(Number));
      if (!lines.length || lines.some((l) => l.length !== 2 || !Number.isInteger(l[0]) || !Number.isInteger(l[1])))
        return fail(400, "Your tray could not be read. Reload the page and try again.");
      const claimed = {};
      for (const [i, q] of lines) {
        const why = checkLine(u.login, i, q, claimed[i] || 0);
        if (why) return fail(409, why);  // nothing booked
        claimed[i] = (claimed[i] || 0) + q;
      }
      const made = lines.map(([i, q]) => book(u.login, i, q));
      save();
      return ok({ codes: made.map((r) => code(r.id)), total: made.reduce((s, r) => s + db.items[r.item].price * r.quantity, 0),
        message: "Reserved. Confirmation email sent (demo: not really). Pay at the counter when you collect." });
    },
    "GET /api/reservations/mine": () => {
      const u = who();
      if (!u) return fail(401, "Please log in.");
      return ok({ reservations: db.reservations.filter((r) => today(r) && r.user === u.login).sort((a, b) => b.id - a.id).map((r) => resJson(r)) });
    },
    "GET /api/reservations/history": () => {
      const u = who();
      if (!u) return fail(401, "Please log in.");
      // newest day first, then later time, then higher id (the server's linked-list merge sort)
      const orders = db.reservations.filter((r) => !today(r) && r.user === u.login)
        .sort((a, b) => b.day - a.day || toMin(b.at) - toMin(a.at) || b.id - a.id)
        .map((r) => ({ code: code(r.id), item: db.items[r.item].name, quantity: r.quantity, status: r.status, day: r.day, reservedAt: r.at }));
      return ok({ count: orders.length, orders });
    },
    "POST /api/reservations/cancel": (p) => {
      const u = who();
      if (!u) return fail(401, "Please log in.");
      const r = db.reservations.find((x) => x.id === Number(p.id) && x.user === u.login && today(x));
      if (!r) return fail(404, "Reservation not found.");
      if (r.status !== "Reserved") return fail(409, "Only reservations with status Reserved can be cancelled.");
      if (NOW >= db.settings.cancel_deadline) return fail(409, "The cancellation deadline has passed.");
      r.status = "Cancelled";
      promote(r.item);
      save();
      return ok({ message: "Reservation " + code(r.id) + " cancelled." });
    },
    "POST /api/waitlist/join": (p) => {
      const u = who();
      if (!u) return fail(401, "Please log in.");
      const i = Number(p.item), q = Number(p.quantity);
      const it = db.items[i];
      if (!it || it.kind !== "fresh") return fail(400, "That dish does not exist.");
      if (remaining(i) > 0) return fail(409, "Portions are still left: reserve one instead.");
      if (wl(i).some((w) => w.user === u.login)) return fail(409, "You are already on this waitlist.");
      if (!(q >= 1 && yours(u.login, i) + q <= db.settings.per_user_limit)) return fail(409, "That would pass the per-person limit.");
      wl(i).push({ user: u.login, quantity: q });
      save();
      return ok({ message: "You are #" + wl(i).length + " on the waitlist for " + it.name + "." });
    },
    "POST /api/waitlist/leave": (p) => {
      const u = who();
      if (!u) return fail(401, "Please log in.");
      db.waitlist[Number(p.item)] = wl(Number(p.item)).filter((w) => w.user !== u.login);
      save();
      return ok({ message: "You left the waitlist." });
    },
    "POST /api/account/password": (p) => {
      const u = who();
      if (!u) return fail(401, "Please log in.");
      if (u.role === "staff") return fail(403, "You do not have access to this page.");
      if (p.current !== u.password) return fail(401, "Your current password is not correct.");
      if (String(p.password || "").length < 8 || p.password === u.password)
        return fail(400, "Choose a new password of 8 to 128 characters, different from the current one.");
      u.password = p.password;
      save();
      return ok({ message: "Password changed. You were signed out on your other devices." });
    },
  };

  // ---- staff (cafeteria) ----------------------------------------------------------
  const staffOnly = (fn) => (p, q) => {
    const u = who();
    if (!u) return fail(401, "Please log in.");
    if (u.role !== "staff") return fail(403, "You do not have access to this page.");
    const out = fn(p, q, u);
    save();
    return out;
  };
  routes["GET /api/staff/overview"] = staffOnly((p, q, u) => ok({
    now: hhmm(NOW), staffName: u.login, emailMode: "outbox",
    settings: Object.assign({}, db.settings, { cutoff: hhmm(db.settings.cutoff), cancel_deadline: hhmm(db.settings.cancel_deadline) }),
    items: db.items.map((it, index) => {
      const base = { index, name: it.name, category: it.category, kind: it.kind, stock: it.stock, price: it.price, image: it.image, available: it.available };
      if (it.kind !== "fresh") return base;
      const waitingRes = db.reservations.filter((r) => live(r) && r.item === index && r.status === "Reserved");
      return Object.assign(base, { limit: it.limit, reserved: reserved(index), remaining: remaining(index),
        waitingPortions: waitingRes.reduce((s, r) => s + r.quantity, 0), queueEntries: waitingRes.length, queueCapacity: 100,
        waitlist: wl(index).length });
    }),
    reminderQueue: db.reservations.filter((r) => live(r) && r.status === "Ready").length, emailsWaiting: 0,
    historySize: db.undo.length, historyCapacity: 50,
    history: db.undo.slice().reverse().map((a) => ({ description: a.description, at: a.at, staff: a.staff })),
  }));
  routes["GET /api/staff/reservations"] = staffOnly((p, q) => {
    const term = String(q.get("q") || "").toLowerCase();
    return ok({ reservations: db.reservations.filter((r) => today(r))
      .filter((r) => !term || r.user.includes(term) || code(r.id).toLowerCase().includes(term))
      .sort((a, b) => a.id - b.id).map((r) => resJson(r, true)) });
  });
  routes["POST /api/staff/ready"] = staffOnly((p, q, u) => {
    const i = Number(p.item);
    if (!db.items[i] || db.items[i].kind !== "fresh") return fail(400, "That dish does not exist.");
    let portions = String(p.portions || "0") === "0" ? Infinity : intIn(p.portions, 1, 1000);
    if (portions === null) return fail(400, "Portions must be a whole number.");
    let n = 0;
    for (const r of db.reservations.filter((x) => live(x) && x.item === i && x.status === "Reserved").sort((a, b) => a.id - b.id)) {
      if (r.quantity > portions) break;  // first come, first served: never skip ahead
      portions -= r.quantity;
      r.status = "Ready";
      r.readyAt = hhmm(NOW);
      n++;
    }
    return ok({ message: n + " reservation(s) for " + db.items[i].name + " marked ready, in the order they were made. Ready emails sent (demo: not really)." });
  });
  routes["POST /api/staff/collected"] = staffOnly((p) => {
    const r = db.reservations.find((x) => x.id === Number(p.id) && today(x));
    if (!r) return fail(404, "Reservation not found.");
    if (r.status !== "Ready") return fail(409, "Only reservations with status Ready can be marked collected.");
    r.status = "Collected";
    return ok({ message: code(r.id) + " marked collected." });
  });
  routes["POST /api/staff/limit"] = staffOnly((p, q, u) => {
    const i = Number(p.item), it = db.items[i];
    const v = intIn(p.value, 0, 100);
    if (!it || it.kind !== "fresh" || v === null) return fail(400, "Enter a whole number from 0 to 100.");
    if (v < reserved(i)) return fail(409, "The limit cannot go below the " + reserved(i) + " portions already reserved.");
    const before = it.limit;
    it.limit = v;
    pushUndo(u.login, "Daily limit of " + it.name + ": " + before + " → " + v, { kind: "limit", i, value: before });
    promote(i);
    return ok({ message: "Daily limit of " + it.name + " is now " + v + "." });
  });
  routes["POST /api/staff/stock"] = staffOnly((p, q, u) => {
    const i = Number(p.item), it = db.items[i];
    const v = intIn(p.value, 0, 10000);
    if (!it || it.kind !== "packaged" || v === null) return fail(400, "Enter a whole number.");
    const before = it.stock;
    it.stock = v;
    pushUndo(u.login, "Stock of " + it.name + ": " + before + " → " + v, { kind: "stock", i, value: before });
    return ok({ message: "Stock of " + it.name + " is now " + v + "." });
  });
  routes["POST /api/staff/settings"] = staffOnly((p, q, u) => {
    const key = String(p.key || "");
    if (!(key in db.settings)) return fail(400, "Unknown setting.");
    const clock = key === "cutoff" || key === "cancel_deadline";
    let v;
    if (clock) {
      const m = /^(\d{2}):(\d{2})$/.exec(String(p.value || ""));
      v = m && Number(m[1]) < 24 && Number(m[2]) < 60 ? Number(m[1]) * 60 + Number(m[2]) : null;
    } else v = intIn(p.value, 1, key === "per_user_limit" ? 10 : 1440);
    if (v === null) return fail(400, "That value is not valid.");
    const before = db.settings[key];
    db.settings[key] = v;
    const show = (x) => (clock ? hhmm(x) : x);
    pushUndo(u.login, "Setting " + key + ": " + show(before) + " → " + show(v), { kind: "setting", key, value: before });
    return ok({ message: "Saved " + key + " = " + show(v) + "." });
  });
  routes["POST /api/staff/undo"] = staffOnly(() => {
    const a = db.undo.pop();
    if (!a) return fail(409, "Nothing to undo.");
    const r = a.revert;
    if (r.kind === "limit") db.items[r.i].limit = r.value;
    if (r.kind === "stock") db.items[r.i].stock = r.value;
    if (r.kind === "setting") db.settings[r.key] = r.value;
    if (r.kind === "price") db.items[r.i].price = r.value;
    if (r.kind === "available") db.items[r.i].available = r.value;
    return ok({ message: "Undone: " + a.description });
  });
  routes["POST /api/staff/items"] = staffOnly((p) => {
    const name = String(p.name || "").trim(), category = String(p.category || "").trim();
    const price = intIn(p.price, 0, 100000), amount = intIn(p.amount, 0, 10000);
    if (!name || !category || name.length > 60 || category.length > 60 || /[|]/.test(name + category) || category[0] === "#" ||
        price === null || amount === null || !["fresh", "packaged"].includes(p.kind))
      return fail(400, "Fill in every field; price and amount are whole numbers.");
    if (db.items.some((it) => it.name.toLowerCase() === name.toLowerCase())) return fail(409, "A dish with that name already exists.");
    if (db.items.length >= 32) return fail(409, "The menu is full (32 items).");
    db.items.push(p.kind === "fresh" ? fresh(name, price, amount) : packaged(category, name, price, amount, "chips.svg"));
    db.items[db.items.length - 1].category = category;
    return ok({ message: name + " added to the menu." });
  });
  routes["POST /api/staff/items/price"] = staffOnly((p, q, u) => {
    const i = Number(p.item), it = db.items[i], v = intIn(p.price, 0, 100000);
    if (!it || v === null) return fail(400, "Enter a whole number of taka.");
    const before = it.price;
    it.price = v;
    pushUndo(u.login, "Price of " + it.name + ": " + before + " → " + v, { kind: "price", i, value: before });
    return ok({ message: "Price of " + it.name + " is now ৳ " + v + "." });
  });
  routes["POST /api/staff/items/available"] = staffOnly((p, q, u) => {
    const i = Number(p.item), it = db.items[i];
    if (!it) return fail(400, "That dish does not exist.");
    const before = it.available;
    it.available = p.available === "1";
    pushUndo(u.login, it.name + (it.available ? " available today" : " not available today"), { kind: "available", i, value: before });
    return ok({ message: it.name + (it.available ? " is available today." : " is hidden from the menu today.") });
  });
  routes["POST /api/staff/items/image"] = staffOnly(() =>
    fail(400, "Picture upload is switched off in the online demo (there is no server to keep the file)."));

  // ---- fetch hook -----------------------------------------------------------------
  const realFetch = window.fetch.bind(window);
  window.fetch = async (input, init) => {
    const url = new URL(typeof input === "string" ? input : input.url, location.href);
    const at = url.pathname.indexOf("/api/");
    if (at < 0) return realFetch(input, init);
    const method = ((init && init.method) || "GET").toUpperCase();
    const route = routes[method + " " + url.pathname.slice(at)];
    let params = {};
    if (init && init.body instanceof URLSearchParams) params = Object.fromEntries(init.body);
    const out = route ? route(params, url.searchParams) : fail(404, "Not found.");
    await new Promise((r) => setTimeout(r, 120));  // feel like a network call
    return new Response(JSON.stringify(out.body), { status: out.status, headers: { "Content-Type": "application/json" } });
  };

  // ---- the demo notice -------------------------------------------------------------
  document.addEventListener("DOMContentLoaded", () => {
    const note = document.createElement("aside");
    note.className = "demo-note";
    note.setAttribute("aria-label", "About this demo");
    const text = document.createElement("p");
    text.textContent = "Online demo: fake data kept in your browser, no emails sent, clock fixed at 08:30. " +
      "The real system is a C++ server. Try ayon@uni.example / demo-student-pass, guard1@uni.example / demo-guard-pass, " +
      "or staff demo.staff / demo-staff-pass at the staff login (" + BASE + "staff/login.html).";
    const reset = document.createElement("button");
    reset.type = "button";
    reset.className = "linkbtn";
    reset.textContent = "Reset demo";
    reset.addEventListener("click", () => {
      try {
        localStorage.removeItem(KEY);
        sessionStorage.clear();
      } catch (e) {
        /* ignore */
      }
      location.href = BASE;
    });
    text.append(" ", reset);
    note.append(text);
    document.body.prepend(note);
  });
})();
