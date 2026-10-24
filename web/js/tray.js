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

let trayCountShown = null;  // the count last drawn, so a change can pulse once

// Draws the tray. highlight: the item index just added or changed (its line
// slides in once).
function renderTray(menu, onChange, highlight) {
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
      onChange(idx);
    });
    const line = el("li", { class: String(highlight) === idx ? "is-new" : null },
      el("span", {}, item.name, el("span", { class: "hint" }, taka(item.price) + " each")),
      select,
      el("button", {
        class: "btn", type: "button", "aria-label": "Remove " + item.name + " from tray",
        onclick: () => {
          line.classList.add("is-leaving");
          setTimeout(() => { Tray.set(idx, 0); onChange(); }, 120);
        },
      }, "Remove"));
    return line;
  });
  document.getElementById("tray-lines").replaceChildren(...rows);
  document.getElementById("tray-empty").hidden = rows.length > 0;
  document.getElementById("tray-total-row").hidden = rows.length === 0;
  document.getElementById("tray-total").textContent = "৳ " + total;
  const reserveAll = document.getElementById("reserve-all");
  reserveAll.hidden = rows.length === 0;
  if (reserveAll.getAttribute("aria-busy") !== "true") {
    reserveAll.disabled = !menu.open;
    reserveAll.textContent = menu.open ? "Reserve all" : "Orders are closed for today";
  }
  const badge = document.getElementById("tray-count");
  badge.textContent = String(count);
  if (trayCountShown !== null && trayCountShown !== count) {
    badge.classList.add("is-pulse");
    setTimeout(() => badge.classList.remove("is-pulse"), 120);
  }
  trayCountShown = count;
  // Phones and tablets: the tray is below every dish, so a bar at the bottom of
  // the screen shows what is in it and jumps there (CSS hides it on wide screens).
  const bar = document.getElementById("tray-bar");
  bar.hidden = count === 0;
  document.getElementById("tray-bar-text").textContent =
    count + (count === 1 ? " dish" : " dishes") + ", ৳ " + total;
  bar.setAttribute("aria-label", "Review tray: " + count + (count === 1 ? " dish" : " dishes") + ", " + total + " taka");
}

// Sends the whole tray. The button stays locked until the server answers, so a
// second press cannot reserve twice.
async function reserveTray(onDone) {
  const items = Object.entries(Tray.lines()).map(([idx, qty]) => idx + ":" + qty).join(",");
  const button = document.getElementById("reserve-all");
  if (!items || button.getAttribute("aria-busy") === "true") return;
  const done = busy(button, "Reserving…");
  say("tray-msg", "");
  let r;
  try {
    r = await api.post("/api/reservations/batch", { items });
  } catch (e) {
    r = { ok: false, error: "Could not reach the cafeteria. Check your connection and try again." };
  }
  done();
  if (!requireLogin(r, "/login.html")) return;
  if (r.ok) {
    Tray.clear();
    say("tray-msg", r.message + " Codes: " + r.codes.join(", ") + ". Total " + taka(r.total) + ".", true);
  } else {
    say("tray-msg", r.error + " Nothing was reserved.", false);
  }
  onDone();
}
