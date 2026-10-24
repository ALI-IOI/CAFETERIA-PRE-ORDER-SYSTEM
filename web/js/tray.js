"use strict";

// The tray lives in the browser (sessionStorage) until "Reserve all" sends it
// to /api/reservations/batch, which reserves every dish or none.
const Tray = {
  lines() {
    return store.get("tray", {}); // { itemIndex: quantity }
  },
  set(itemIndex, qty) {
    const t = Tray.lines();
    if (qty > 0) t[itemIndex] = qty;
    else delete t[itemIndex];
    store.set("tray", t);
  },
  clear() {
    store.set("tray", {});
  },
  // Most portions this person may still add for an item.
  room(item, menu) {
    return Math.max(0, Math.min(menu.perUserLimit - item.yours, item.remaining));
  },
  // Drops dishes that disappeared, sold out or no longer fit; returns notes.
  reconcile(menu) {
    const notes = [];
    const byIndex = new Map(menu.items.map((i) => [String(i.index), i]));
    const t = Tray.lines();
    for (const [idx, qty] of Object.entries(t)) {
      const item = byIndex.get(idx);
      if (!item || item.kind !== "fresh") {
        delete t[idx];
        notes.push("A dish in your tray is no longer available and was removed.");
        continue;
      }
      const room = Tray.room(item, menu);
      if (room === 0) {
        delete t[idx];
        notes.push(item.name + " was removed from your tray: none left for you.");
      } else if (qty > room) {
        t[idx] = room;
        notes.push(item.name + " in your tray was reduced to " + room + ".");
      }
    }
    store.set("tray", t);
    return notes;
  },
};

function renderTray(menu, onChange) {
  const byIndex = new Map(menu.items.map((i) => [String(i.index), i]));
  const entries = Object.entries(Tray.lines()).filter(([idx]) => byIndex.has(idx));
  let total = 0, count = 0;
  const rows = entries.map(([idx, qty]) => {
    const item = byIndex.get(idx);
    total += item.price * qty;
    count += qty;
    const selectId = "tray-qty-" + idx;
    const select = el("select", { id: selectId, "aria-label": "Portions of " + item.name });
    for (let q = 1; q <= Math.max(qty, Tray.room(item, menu)); q++) {
      select.append(el("option", { value: q, selected: q === qty }, q));
    }
    select.addEventListener("change", () => {
      Tray.set(idx, Number(select.value));
      onChange();
    });
    return el("li", {},
      el("span", {}, item.name, el("br"), el("span", { class: "hint" }, taka(item.price) + " each")),
      select,
      el("button", {
        class: "btn btn--ghost", type: "button", "aria-label": "Remove " + item.name + " from tray",
        onclick: () => { Tray.set(idx, 0); onChange(); },
      }, "Remove"));
  });
  document.getElementById("tray-lines").replaceChildren(...rows);
  document.getElementById("tray-empty").hidden = rows.length > 0;
  document.getElementById("tray-total-row").hidden = rows.length === 0;
  document.getElementById("tray-total").textContent = "৳ " + total;
  const reserveAll = document.getElementById("reserve-all");
  reserveAll.hidden = rows.length === 0;
  reserveAll.disabled = !menu.open;
  document.getElementById("tray-count").textContent = String(count);
  // Phones and tablets: the tray is below every dish, so a bar at the bottom of
  // the screen shows what is in it and jumps there (CSS hides it on wide screens).
  const bar = document.getElementById("tray-bar");
  bar.hidden = count === 0;
  document.getElementById("tray-bar-text").textContent =
    count + (count === 1 ? " dish" : " dishes") + ", \u09f3 " + total;
  bar.setAttribute("aria-label", "Review tray: " + count + (count === 1 ? " dish" : " dishes") + ", " + total + " taka");
}

async function reserveTray(onDone) {
  const items = Object.entries(Tray.lines()).map(([idx, qty]) => idx + ":" + qty).join(",");
  if (!items) return;
  const r = await api.post("/api/reservations/batch", { items });
  if (!requireLogin(r, "/login.html")) return;
  if (r.ok) {
    Tray.clear();
    say("msg", r.message + " Codes: " + r.codes.join(", ") + ". Total " + taka(r.total) + ".", true);
  } else {
    say("msg", r.error, false);
  }
  onDone();
}
