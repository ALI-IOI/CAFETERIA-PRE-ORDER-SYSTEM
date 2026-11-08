"use strict";

// "20261024" -> "24/10/2026"
function dayText(day) {
  const s = String(day);
  return s.slice(6, 8) + "/" + s.slice(4, 6) + "/" + s.slice(0, 4);
}

let shownStatus = null;  // code -> status at the last draw, so a change can fade in

async function load() {
  const restoreFocus = keepFocus();
  const state = document.getElementById("rows-state");
  const table = document.getElementById("today-table");
  if (shownStatus === null) { listState(state, "loading"); table.hidden = true; }
  let r;
  try {
    r = await api.get("/api/reservations/mine");
  } catch (e) {
    r = { ok: false, status: 0 };
  }
  if (!requireLogin(r, "/login.html")) return;
  if (!r.ok || !Array.isArray(r.reservations)) {
    listState(state, "error", "Couldn’t load your reservations. Check your connection and try again.", load);
    table.hidden = true;
    document.getElementById("empty").hidden = true;  // the list is unknown, not empty
    restoreFocus();
    return;
  }
  listState(state, null);
  const before = shownStatus || {};
  shownStatus = {};
  const rows = r.reservations.map((res) => {
    shownStatus[res.code] = res.status;
    const cancel = res.canCancel
      ? el("button", {
          class: "btn btn--danger",
          type: "button",
          "aria-label": "Cancel " + res.code + " (" + res.item + ")",
          onclick: async (e) => {
            const button = e.currentTarget;
            if (button.getAttribute("aria-busy") === "true") return;  // a second press waits for the first
            const done = busy(button, "Cancelling…");
            const c = await api.post("/api/reservations/cancel", { id: res.id });
            done();
            say("msg", c.ok ? c.message : c.error, c.ok);
            load();
          },
        }, "Cancel")
      : "";
    const changed = before[res.code] !== undefined && before[res.code] !== res.status;
    const classes = [res.status === "Ready" ? "is-ready" : "", changed ? "is-changed" : ""].filter(Boolean).join(" ");
    return el("tr", { class: classes || null },
      el("th", { scope: "row" }, res.code),
      el("td", {}, res.tokenText ? el("span", { class: "token-tag" }, res.tokenText) : "–"),
      el("td", {}, res.item),
      el("td", { class: "num" }, res.quantity),
      // ready orders from yesterday stay in this list, so every time carries its date
      el("td", {}, dayText(res.day).slice(0, 5) + " " + res.reservedAt),
      el("td", {}, statusChip(res.status)),
      el("td", {}, res.status === "Ready" ? res.freshUntil : "–"),
      el("td", {}, cancel));
  });
  document.getElementById("rows").replaceChildren(...rows);
  drawTokens(r.reservations);
  table.hidden = rows.length === 0;
  document.getElementById("empty").hidden = rows.length > 0;
  restoreFocus();
}

// One stamp per order token still in play: cooking while any dish is Reserved,
// ready once every remaining dish is Ready. Collected, cancelled and expired
// orders drop out.
function drawTokens(reservations) {
  const byToken = new Map();
  for (const res of reservations) {
    if (!res.token || res.status === "Cancelled") continue;
    const t = byToken.get(res.token) || { text: res.tokenText, cooking: 0, ready: 0, dishes: 0 };
    t.dishes++;
    if (res.status === "Reserved") t.cooking++;
    if (res.status === "Ready") t.ready++;
    byToken.set(res.token, t);
  }
  const stamps = [];
  for (const [, t] of [...byToken].sort((a, b) => a[0] - b[0])) {
    if (t.cooking > 0) {
      stamps.push(tokenStamp("Cooking", t.text, t.ready ? t.ready + " of " + t.dishes + " ready" : "", "", "small"));
    } else if (t.ready > 0) {
      stamps.push(tokenStamp("Ready", t.text, "Collect now", "ready", "small"));
    }
  }
  document.getElementById("tokens").replaceChildren(...stamps);
  document.getElementById("tokens-hint").hidden = stamps.length === 0;
}

// The history comes merge-sorted newest first from the server; each day gets
// its own heading and table, in that order.
function dayGroup(day, orders) {
  const table = el("table", { "aria-label": "Orders on " + dayText(day) },
    el("thead", {}, el("tr", {},
      el("th", { scope: "col" }, "Reservation"), el("th", { scope: "col" }, "Food"),
      el("th", { scope: "col", class: "num" }, "Qty"), el("th", { scope: "col" }, "Reserved at"),
      el("th", { scope: "col" }, "Status"))),
    el("tbody", {}, ...orders.map((o) => el("tr", {},
      el("th", { scope: "row" }, o.code),
      el("td", {}, o.item),
      el("td", { class: "num" }, o.quantity),
      el("td", {}, o.reservedAt),
      el("td", {}, statusChip(o.status))))));
  labelTableCells(table);
  return el("section", { class: "day-group" }, el("h3", {}, dayText(day)), el("div", { class: "card table-wrap" }, table));
}

async function loadHistory() {
  const box = document.getElementById("history-rows");
  listState(box, "loading");
  let h;
  try {
    h = await api.get("/api/reservations/history");
  } catch (e) {
    h = { ok: false, status: 0 };
  }
  if (!requireLogin(h, "/login.html")) return;
  if (!h.ok || !Array.isArray(h.orders)) {
    listState(box, "error", "Couldn’t load your order history. Check your connection and try again.", loadHistory);
    return;
  }
  const groups = [];
  for (const o of h.orders) {
    if (!groups.length || groups[groups.length - 1].day !== o.day) groups.push({ day: o.day, orders: [] });
    groups[groups.length - 1].orders.push(o);
  }
  box.replaceChildren(...groups.map((g) => dayGroup(g.day, g.orders)));
  document.getElementById("history-empty").hidden = groups.length > 0;
}

wireLogout("logout", "/");
load();
loadHistory();
fillAnnouncement();
