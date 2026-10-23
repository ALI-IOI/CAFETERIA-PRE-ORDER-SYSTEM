"use strict";

function dayText(day) {
  const s = String(day);
  return s.slice(6, 8) + "/" + s.slice(4, 6);
}

async function load() {
  const r = await api.get("/api/reservations/mine");
  if (!requireLogin(r, "/login.html")) return;
  const rows = r.reservations.map((res) => {
    const cancel = res.canCancel
      ? el("button", {
          class: "btn btn--danger",
          type: "button",
          "aria-label": "Cancel " + res.code + " (" + res.item + ")",
          onclick: async () => {
            const c = await api.post("/api/reservations/cancel", { id: res.id });
            say("msg", c.ok ? c.message : c.error, c.ok);
            load();
          },
        }, "Cancel")
      : "";
    return el("tr", {},
      el("th", { scope: "row" }, res.code),
      el("td", {}, res.item),
      el("td", { class: "num" }, res.quantity),
      el("td", {}, dayText(res.day) + " " + res.reservedAt),
      el("td", {}, statusChip(res.status)),
      el("td", {}, res.status === "Ready" ? res.freshUntil : "—"),
      el("td", {}, cancel));
  });
  document.getElementById("rows").replaceChildren(...rows);
  document.getElementById("empty").hidden = rows.length > 0;
}

async function loadHistory() {
  const h = await api.get("/api/reservations/history");
  if (!requireLogin(h, "/login.html")) return;
  if (!h.ok) {
    say("msg", h.error);
    return;
  }
  const rows = h.orders.map((o) => el("tr", {},
    el("th", { scope: "row" }, o.code),
    el("td", {}, dayText(o.day)),
    el("td", {}, o.item),
    el("td", { class: "num" }, o.quantity),
    el("td", {}, o.reservedAt),
    el("td", {}, statusChip(o.status))));
  document.getElementById("history-rows").replaceChildren(...rows);
  document.getElementById("history-empty").hidden = rows.length > 0;
}

wireLogout("logout", "/");
load();
loadHistory();
fillAnnouncement();
