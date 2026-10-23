"use strict";

// Today's menu: category tabs, search and sort run in the browser on the data
// from /api/menu. Fresh dishes go into the tray; sold-out ones offer the waitlist.
let menu = null;
let activeCategory = "All";

async function loadProfile() {
  const me = await api.get("/api/me");
  if (!requireLogin(me, "/login.html")) return;
  document.getElementById("profile").replaceChildren(
    el("span", { class: "role-badge" }, roleLabel(me.role)),
    el("strong", {}, me.name),
    me.role === "student"
      ? el("span", {}, "Student ID: " + me.universityId)
      : el("span", {}, "Department: " + (me.department || "—")),
  );
}

function freshControls(item) {
  const room = Tray.room(item, menu);
  const inTray = Tray.lines()[item.index] || 0;
  const selectId = "qty-" + item.index;
  const select = el("select", { id: selectId, disabled: !menu.open || room === 0 });
  for (let q = 1; q <= Math.max(1, room); q++) select.append(el("option", { value: q }, q));
  const button = el("button", { class: "btn", type: "submit", disabled: !menu.open || room === 0 },
    inTray ? "Update tray" : "Add to tray");
  const form = el("form", { class: "row", "aria-label": "Add " + item.name + " to your tray" },
    el("div", { class: "field" }, el("label", { for: selectId }, "Portions"), select), button);
  form.addEventListener("submit", (e) => {
    e.preventDefault();
    Tray.set(item.index, Number(select.value));
    say("msg", item.name + " x " + select.value + " is in your tray.", true);
    render();
  });
  return form;
}

// Sold out: join the waitlist, or show your place in line.
function waitlistBox(item) {
  const box = el("div", { class: "waitlist" });
  if (item.yourPosition > 0) {
    box.append(
      el("p", { class: "dish__meta" }, el("strong", {}, "You are #" + item.yourPosition + " on the waitlist."),
        " If a portion becomes free before " + menu.cutoff + ", it is reserved for you and we email you."),
      el("button", {
        class: "btn btn--ghost", type: "button",
        onclick: async () => {
          const r = await api.post("/api/waitlist/leave", { item: item.index });
          say("msg", r.ok ? r.message : r.error, r.ok);
          load();
        },
      }, "Leave waitlist"));
    return box;
  }
  const room = menu.perUserLimit - item.yours;
  if (!menu.open || room < 1) return box;
  const id = "wait-" + item.index;
  const select = el("select", { id });
  for (let q = 1; q <= room; q++) select.append(el("option", { value: q }, q));
  const form = el("form", { class: "row", "aria-label": "Join the " + item.name + " waitlist" },
    el("div", { class: "field" }, el("label", { for: id }, "Portions wanted"), select),
    el("button", { class: "btn", type: "submit" }, "Join waitlist"));
  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    const r = await api.post("/api/waitlist/join", { item: item.index, quantity: select.value });
    say("msg", r.ok ? r.message : r.error, r.ok);
    load();
  });
  box.append(el("p", { class: "dish__meta" },
    item.waiting > 0 ? item.waiting + " already waiting (first come, first served)." : "Nobody is waiting yet."), form);
  return box;
}

function dishCard(item) {
  const fresh = item.kind === "fresh";
  const soldOut = fresh ? item.remaining <= 0 : item.stock <= 0;
  const img = el("img", { class: "dish__img", src: item.image, alt: "Picture of " + item.name, loading: "lazy" });
  const body = el("div", { class: "dish__body" },
    el("div", { class: "dish__top" }, el("h3", { class: "dish__name" }, item.name), el("span", { class: "price" }, taka(item.price))));
  if (fresh) {
    const pct = item.limit > 0 ? Math.round(((item.limit - item.remaining) / item.limit) * 100) : 100;
    const meter = el("div", { class: "meter", role: "img", "aria-label": item.remaining + " of " + item.limit + " portions left" }, el("span"));
    meter.firstChild.style.width = pct + "%";
    body.append(
      el("p", { class: "dish__meta" }, el("span", { class: "dish__left" }, Math.max(0, item.remaining) + " portions left"),
        ", up to " + menu.perUserLimit + " each" + (item.yours ? ", you have " + item.yours : "")),
      meter);
    if (!menu.open) body.append(el("p", { class: "dish__meta" }, el("strong", {}, "Reservations closed for today.")));
    if (soldOut || item.yourPosition > 0) body.append(waitlistBox(item));
    if (!soldOut) body.append(freshControls(item));
  } else {
    body.append(el("p", { class: "dish__meta" },
      item.stock > 0 ? item.stock + " in stock, buy at the counter" : el("span", { class: "out" }, "Out of stock")));
  }
  return el("article", { class: "dish" + (soldOut ? " dish--soldout" : "") }, img, body);
}

function renderTabs() {
  const categories = ["All", ...new Set(menu.items.map((i) => i.category))];
  if (!categories.includes(activeCategory)) activeCategory = "All";
  document.getElementById("tabs").replaceChildren(...categories.map((c) => el("li", {},
    el("button", {
      class: "tab", type: "button", "aria-pressed": String(c === activeCategory),
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

function render() {
  renderTabs();
  const items = visibleItems();
  document.getElementById("dishes").replaceChildren(...items.map(dishCard));
  document.getElementById("count").textContent = items.length + (items.length === 1 ? " item" : " items");
  renderTray(menu, render);
}

async function load() {
  const m = await api.get("/api/menu");
  if (!requireLogin(m, "/login.html")) return;
  menu = m;
  const banner = document.getElementById("banner");
  banner.className = "banner" + (menu.open ? "" : " banner--closed");
  banner.textContent = menu.open
    ? "Reserve before " + menu.cutoff + ". You can cancel until " + menu.cancelDeadline +
      ". Food stays fresh for about " + menu.freshness + " after it is ready. (Server time " + menu.now + ")"
    : "Reservations closed at " + menu.cutoff + " today. (Server time " + menu.now + ")";
  const notes = Tray.reconcile(menu);
  if (notes.length) say("msg", notes.join(" "));
  render();
}

document.getElementById("search").addEventListener("input", render);
document.getElementById("sort").addEventListener("change", render);
document.getElementById("reserve-all").addEventListener("click", () => reserveTray(load));
wireLogout("logout", "/");
loadProfile();
load();
fillAnnouncement();
