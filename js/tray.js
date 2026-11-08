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
  // The coupon checked against this tray: { code, occasion, kind, value, text }.
  // The server checks it again when the tray is reserved.
  coupon() {
    return store.get("coupon", null);
  },
  setCoupon(c) {
    store.set("coupon", c);
  },
  // What the coupon takes off: whole taka, never more than the dishes.
  discount(subtotal) {
    const c = Tray.coupon();
    if (!c || subtotal <= 0) return 0;
    const off = c.kind === "percent" ? Math.floor((subtotal * c.value) / 100) : c.value;
    return Math.min(off, subtotal);
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
  let subtotal = 0, count = 0;
  const rows = entries.map(([idx, qty]) => {
    const item = byIndex.get(idx);
    subtotal += item.price * qty;
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
        id: "tray-remove-" + idx, class: "btn", type: "button", "aria-label": "Remove " + item.name + " from tray",
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
  document.getElementById("tray-coupon").hidden = rows.length === 0;
  const coupon = Tray.coupon();
  const discount = Tray.discount(subtotal);
  const total = subtotal - discount;
  document.getElementById("tray-discount-row").hidden = rows.length === 0 || !coupon;
  if (coupon) {
    document.getElementById("tray-sub").textContent = "৳ " + subtotal;
    document.getElementById("tray-discount-label").replaceChildren(
      el("span", {}, "Coupon ", el("strong", {}, coupon.code), " (" + coupon.text + ") "),
      el("button", { id: "coupon-remove", class: "btn", type: "button", "aria-label": "Remove coupon " + coupon.code,
        onclick: () => { Tray.setCoupon(null); say("coupon-msg", "Coupon removed."); onChange(); } }, "Remove"));
    document.getElementById("tray-discount").textContent = "− ৳ " + discount;
  }
  document.getElementById("tray-total").textContent = "৳ " + total;
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
    setTimeout(() => badge.classList.remove("is-pulse"), 100);  // 100 ms up, 100 ms back
  }
  trayCountShown = count;
  // Phones and tablets: the tray is below every dish, so a bar at the bottom of
  // the screen shows what is in it and jumps there (CSS hides it on wide screens).
  const bar = document.getElementById("tray-bar");
  bar.hidden = count === 0;
  document.getElementById("tray-bar-text").textContent =
    count + (count === 1 ? " dish" : " dishes") + ", ৳ " + total;
  bar.setAttribute("aria-label", "Review tray: " + count + (count === 1 ? " dish" : " dishes") + ", " + total + " taka");
}

// Sends the whole tray. The button stays locked until the server answers, so a
// second press cannot reserve twice.
async function reserveTray(onDone) {
  const items = trayItems();
  const coupon = Tray.coupon();
  const button = document.getElementById("reserve-all");
  if (!items || button.getAttribute("aria-busy") === "true") return;
  const done = busy(button, "Reserving…");
  say("tray-msg", "");
  document.getElementById("tray-token").hidden = true;
  let r;
  try {
    r = await api.post("/api/reservations/batch", { items, coupon: coupon ? coupon.code : "" });
  } catch (e) {
    r = { ok: false, error: "Could not reach the cafeteria. Check your connection and try again." };
  }
  done();
  if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/login.html")) return;
  if (r.ok) {
    Tray.clear();
    Tray.setCoupon(null);
    say("coupon-msg", "");
    say("tray-msg", r.message + " Codes: " + r.codes.join(", ") + ". Total " + taka(r.total) +
      (r.discount > 0 ? " after " + taka(r.discount) + " off" : "") + ".", true);
    const box = document.getElementById("tray-token");
    box.replaceChildren(tokenStamp("Your token", r.tokenText, "", "", "small"),
      el("p", {}, "Keep this number. Watch for it on the ", el("a", { href: "/CAFETERIA-PRE-ORDER-SYSTEM/board.html" }, "token board"), "."));
    box.hidden = false;
  } else {
    say("tray-msg", r.error + " Nothing was reserved.", false);
  }
  onDone();
}

function trayItems() {
  return Object.entries(Tray.lines()).map(([idx, qty]) => idx + ":" + qty).join(",");
}

// Checks a code against the tray and keeps it for the tray when it is good.
async function applyCoupon(code, onChange) {
  code = code.trim();
  if (!code) {
    say("coupon-msg", "Type a coupon code first.", false);
    return;
  }
  const button = document.getElementById("coupon-apply");
  if (button.getAttribute("aria-busy") === "true") return;
  const done = busy(button, "Checking…");
  let r;
  try {
    r = await api.post("/api/coupons/check", { code, items: trayItems() });
  } catch (e) {
    r = { ok: false, error: "Could not reach the cafeteria. Check your connection and try again." };
  }
  done();
  if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/login.html")) return;
  if (!r.ok) {
    say("coupon-msg", r.error, false);
    return;
  }
  Tray.setCoupon({ code: r.code, occasion: r.occasion, kind: r.kind, value: r.value, text: r.text });
  document.getElementById("coupon-code").value = "";
  say("coupon-msg", r.code + " applied for " + r.occasion + ": " + r.text + ".", true);
  onChange();
}

// The coupons this person may use today, each with a button that applies it.
function renderOffers(list, onChange) {
  const offers = (list || []).map((c) => el("li", {},
    el("span", {}, el("strong", {}, c.code), " " + c.occasion + ": " + c.text + (c.to ? ", until " + c.to : "")),
    el("button", { class: "btn", type: "button", "aria-label": "Use coupon " + c.code,
      onclick: () => applyCoupon(c.code, onChange) }, "Use")));
  document.getElementById("offers").replaceChildren(...offers);
  document.getElementById("offers-h").hidden = offers.length === 0;
}
