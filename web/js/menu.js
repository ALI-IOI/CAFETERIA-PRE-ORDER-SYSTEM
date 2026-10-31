"use strict";

// Today's menu: category tabs, search and sort run in the browser on the data
// from /api/menu. Fresh dishes go into the tray; sold-out ones offer the waitlist
// (a first-come, first-served queue on the server).
let menu = null;
let activeCategory = "All";
const FEW = 3;  // "Only N left" below this many portions

async function loadProfile() {
  const me = await api.get("/api/me");
  if (!requireLogin(me, "/login.html")) return;
  document.getElementById("profile").replaceChildren(
    el("span", { class: "role-badge" }, roleLabel(me.role)),
    el("strong", {}, me.name),
    me.role === "student"
      ? el("span", { class: "num" }, "Student ID: " + me.universityId)
      : el("span", {}, "Department: " + (me.department || "not given")),
  );
}

function freshControls(item) {
  const room = Tray.room(item, menu);
  const inTray = Tray.lines()[item.index] || 0;
  const selectId = "qty-" + item.index;
  const select = el("select", { id: selectId, disabled: !menu.open || room === 0 });
  for (let q = 1; q <= Math.max(1, room); q++) select.append(el("option", { value: q, selected: q === inTray }, q));
  const button = el("button", { id: "add-" + item.index, class: "btn", type: "submit", disabled: !menu.open || room === 0,
    "aria-describedby": "dish-" + item.index }, inTray ? "Update tray" : "Add to tray");
  const form = el("form", { class: "row" },
    el("div", { class: "field" }, el("label", { for: selectId }, "Portions"), select), button);
  form.addEventListener("submit", (e) => {
    e.preventDefault();
    Tray.set(item.index, Number(select.value));
    say("tray-msg", "");  // codes from an earlier reservation no longer apply
    say("msg", item.name + ", " + select.value + (select.value === "1" ? " portion" : " portions") + ", is in your tray.", true);
    render(String(item.index));
  });
  return form;
}

// Sold out: join the waitlist, or show your place in it.
function waitlistBox(item) {
  const box = el("div", { class: "waitlist" });
  if (item.yourPosition > 0) {
    const leave = el("button", { id: "leave-" + item.index, class: "btn btn--block", type: "button",
      "aria-describedby": "dish-" + item.index }, "Leave waitlist");
    leave.addEventListener("click", async () => {
      const done = busy(leave, "Leaving…");
      const r = await api.post("/api/waitlist/leave", { item: item.index });
      done();
      say("msg", r.ok ? r.message : r.error, r.ok);
      load();
    });
    box.append(
      el("p", { class: "dish__flag" }, "You’re number " + item.yourPosition + " on the waitlist."),
      el("p", { class: "dish__meta" }, "If a portion becomes free before " + menu.cutoff + ", it is reserved for you and we email you."),
      leave);
    return box;
  }
  const room = menu.perUserLimit - item.yours;
  if (!menu.open || room < 1) return box;
  const id = "wait-" + item.index;
  const select = el("select", { id });
  for (let q = 1; q <= room; q++) select.append(el("option", { value: q }, q));
  const join = el("button", { id: "join-" + item.index, class: "btn", type: "submit",
    "aria-describedby": "dish-" + item.index }, "Join waitlist");
  const form = el("form", { class: "row" },
    el("div", { class: "field" }, el("label", { for: id }, "Portions wanted"), select), join);
  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    const done = busy(join, "Joining…");
    const r = await api.post("/api/waitlist/join", { item: item.index, quantity: select.value });
    done();
    say("msg", r.ok ? r.message : r.error, r.ok);
    load();
  });
  box.append(el("p", { class: "dish__meta" },
    item.waiting > 0 ? item.waiting + " already waiting, first come, first served." : "Nobody is waiting yet."), form);
  return box;
}

function dishCard(item) {
  const fresh = item.kind === "fresh";
  const left = fresh ? Math.max(0, item.remaining) : item.stock;
  const soldOut = left <= 0;
  const few = fresh && !soldOut && left < FEW;
  const inTray = fresh ? Tray.lines()[item.index] || 0 : 0;
  const img = el("img", { class: "dish__img", src: item.image, alt: "", loading: "lazy", width: 300, height: 200 });
  const body = el("div", { class: "dish__body" },
    el("div", { class: "dish__top" },
      el("h3", { class: "dish__name", id: "dish-" + item.index }, item.name),
      el("span", { class: "leader", "aria-hidden": "true" }),
      el("span", { class: item.price > 0 ? "price" : "price price--ask" }, taka(item.price))));
  if (fresh) {
    const pct = item.limit > 0 ? Math.round((left / item.limit) * 100) : 0;  // the bar is what is left
    const meter = el("div", { class: "meter", role: "img", "aria-label": left + " of " + item.limit + " portions left" }, el("span"));
    meter.firstChild.style.width = pct + "%";
    const leftText = soldOut ? "Sold out" : few ? "Only " + left + " left" : left + " portions left";
    body.append(
      el("p", { class: "dish__meta" }, el("span", { class: "dish__left" }, leftText),
        ", up to " + menu.perUserLimit + " each" + (item.yours ? ", you have " + item.yours : "")),
      meter);
    if (inTray) body.append(el("p", { class: "dish__flag" }, "In tray: " + inTray));
    if (!menu.open) body.append(el("p", { class: "dish__flag" }, "Reservations closed for today."));
    if (soldOut || item.yourPosition > 0) {
      const box = waitlistBox(item);
      if (box.children.length) body.append(box);  // nothing to offer: no empty box
    }
    if (!soldOut) body.append(freshControls(item));
  } else {
    body.append(el("p", { class: "dish__meta" },
      soldOut ? el("span", { class: "out" }, "Out of stock") : left + " in stock, buy at the counter"));
  }
  const classes = ["dish", soldOut ? "dish--soldout" : "", few ? "is-few" : "", inTray ? "is-in-tray" : ""].filter(Boolean);
  return el("article", { class: classes.join(" ") }, img, body);
}

function renderTabs() {
  const categories = ["All", ...new Set(menu.items.map((i) => i.category))];
  if (!categories.includes(activeCategory)) activeCategory = "All";
  document.getElementById("tabs").replaceChildren(...categories.map((c) => el("li", {},
    el("button", {
      id: "tab-" + categories.indexOf(c), class: "tab", type: "button", "aria-pressed": String(c === activeCategory),
      onclick: () => { activeCategory = c; render(); },
    }, c))));
}

function visibleItems() {
  const q = document.getElementById("search").value.trim().toLowerCase();
  const sort = document.getElementById("sort").value;
  const left = (i) => (i.kind === "fresh" ? i.remaining : i.stock);
  return menu.items
    .filter((i) => activeCategory === "All" || i.category === activeCategory)
    .filter((i) => !q || i.name.toLowerCase().includes(q))
    .sort((a, b) =>
      sort === "price" ? a.price - b.price || a.name.localeCompare(b.name)
        : sort === "left" ? left(b) - left(a) || a.name.localeCompare(b.name)
          : sort === "name" ? a.name.localeCompare(b.name)
            : a.index - b.index);  // menu order: the cafeteria's own order, fresh dishes first
}

// highlight: the item index whose tray line just changed.
function render(highlight) {
  if (!menu) return;  // search or sort before the menu has loaded
  const restoreFocus = keepFocus();
  renderTabs();
  const items = visibleItems();
  const dishes = document.getElementById("dishes");
  if (menu.items.length === 0) listState(dishes, "empty", menu.open
    ? "Nothing is on the menu yet today. Check again later this morning."
    : "Nothing was on the menu today. You can order again tomorrow morning.");
  else if (items.length === 0) listState(dishes, "empty", "No dishes match. Clear the search or pick another category.");
  else dishes.replaceChildren(...items.map(dishCard));
  // a live region: changed only when the number changes, so a tray change is not announced again
  const count = document.getElementById("count");
  const countText = items.length + (items.length === 1 ? " item" : " items");
  if (count.textContent !== countText) count.textContent = countText;
  renderTray(menu, render, highlight);
  restoreFocus();
}

function showStatus() {
  const banner = document.getElementById("banner");
  banner.className = "banner" + (menu.open ? "" : " banner--closed");
  banner.textContent = menu.open
    ? "Reserve before " + menu.cutoff + ". You can cancel until " + menu.cancelDeadline +
      ". Food stays fresh for about " + menu.freshness + " after it is ready. Server time " + menu.now + "."
    : "Orders closed at " + menu.cutoff + " today. You can order again tomorrow morning. Server time " + menu.now + ".";
}

async function load() {
  const dishes = document.getElementById("dishes");
  if (!menu) listState(dishes, "loading");
  let m;
  try {
    m = await api.get("/api/menu");
  } catch (e) {
    m = { ok: false, status: 0 };
  }
  if (!requireLogin(m, "/login.html")) return;
  if (!m.ok || !Array.isArray(m.items)) {
    listState(dishes, "error", "Couldn’t load today’s menu. Check your connection and try again.", load);
    document.getElementById("count").textContent = "";
    return;
  }
  menu = m;
  showStatus();
  const notes = Tray.reconcile(menu);
  if (notes.length) say("msg", notes.join(" "));
  render();
}

document.getElementById("search").addEventListener("input", () => render());
document.getElementById("sort").addEventListener("change", () => render());
document.getElementById("reserve-all").addEventListener("click", () => reserveTray(load));
wireLogout("logout", "/");
loadProfile();
load();
fillAnnouncement();
