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

wireLogout("logout", "/");
load();
