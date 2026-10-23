"use strict";

// Students and teachers share the reservation rules; only the profile differs
// (student ID for students, department for teachers).
async function loadProfile() {
  const me = await api.get("/api/me");
  if (!requireLogin(me, "/")) return;
  const box = document.getElementById("profile");
  box.replaceChildren(
    el("span", { class: "role-badge" }, me.role === "teacher" ? "Teacher" : "Student"),
    el("strong", {}, me.name),
    me.role === "teacher"
      ? el("span", {}, "Department: " + (me.department || "—"))
      : el("span", {}, "Student ID: " + me.universityId),
  );
}

function freshCard(item, menu) {
  const soldOut = item.remaining <= 0;
  const canReserve = menu.open && !soldOut && item.yours < menu.perUserLimit;
  const maxQty = Math.max(1, Math.min(menu.perUserLimit - item.yours, item.remaining));
  const qtyId = "qty-" + item.index;

  const select = el("select", { id: qtyId, name: "quantity", disabled: !canReserve });
  for (let q = 1; q <= maxQty; q++) select.append(el("option", { value: q }, q));

  let reason = "";
  if (!menu.open) reason = "Reservations closed for today.";
  else if (soldOut) reason = "Sold out for today.";
  else if (item.yours >= menu.perUserLimit) reason = "You have reached your limit for this item.";

  const form = el(
    "form",
    { class: "row", "aria-label": "Reserve " + item.name },
    el("div", { class: "field" }, el("label", { for: qtyId }, "Portions"), select),
    el("button", { class: "btn", type: "submit", disabled: !canReserve }, "Reserve"),
  );
  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    const r = await api.post("/api/reservations", { item: item.index, quantity: select.value });
    say("msg", r.ok ? r.message : r.error, r.ok);
    loadMenu();
  });

  return el(
    "article",
    { class: "card food" + (soldOut ? " food--soldout" : "") },
    el("h3", { class: "food__name" }, item.name),
    el("p", { class: "food__left" }, String(Math.max(item.remaining, 0)), el("small", {}, " portions left")),
    el("p", { class: "food__meta" },
      item.yours > 0 ? "You have reserved " + item.yours + " today." : "Up to " + menu.perUserLimit + " per person."),
    reason ? el("p", { class: "food__meta" }, el("strong", {}, reason)) : null,
    // Still waiting while portions are free (the person ahead needs more than
    // are left): show both. Reserving directly takes you off the waitlist.
    soldOut ? waitlistBox(item, menu) : item.yourPosition > 0 ? el("div", {}, form, waitlistBox(item, menu)) : form,
  );
}

// Sold out: join the waitlist, or show your place in line (addition #1).
function waitlistBox(item, menu) {
  const box = el("div", { class: "waitlist" });
  if (item.yourPosition > 0) {
    box.append(
      el("p", { class: "food__meta" },
        el("strong", {}, "You are #" + item.yourPosition + " on the waitlist."),
        " If a portion becomes free before " + menu.cutoff + ", it is reserved for you and we email you."),
      el("button", {
        class: "btn btn--ghost", type: "button",
        onclick: async () => {
          const r = await api.post("/api/waitlist/leave", { item: item.index });
          say("msg", r.ok ? r.message : r.error, r.ok);
          loadMenu();
        },
      }, "Leave waitlist"));
    return box;
  }
  const room = menu.perUserLimit - item.yours;
  if (!menu.open || room < 1) return box;
  const id = "wait-" + item.index;
  const select = el("select", { id, name: "quantity" });
  for (let q = 1; q <= room; q++) select.append(el("option", { value: q }, q));
  const form = el("form", { class: "row", "aria-label": "Join the " + item.name + " waitlist" },
    el("div", { class: "field" }, el("label", { for: id }, "Portions wanted"), select),
    el("button", { class: "btn", type: "submit" }, "Join waitlist"));
  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    const r = await api.post("/api/waitlist/join", { item: item.index, quantity: select.value });
    say("msg", r.ok ? r.message : r.error, r.ok);
    loadMenu();
  });
  box.append(
    el("p", { class: "food__meta" },
      item.waiting > 0 ? item.waiting + " already waiting (first come, first served)." : "Nobody is waiting yet."),
    form);
  return box;
}

function packagedList(items) {
  const byCategory = new Map();
  for (const it of items) {
    if (!byCategory.has(it.category)) byCategory.set(it.category, []);
    byCategory.get(it.category).push(it);
  }
  const sections = [];
  for (const [category, list] of byCategory) {
    sections.push(
      el("section", {},
        el("h3", {}, category),
        el("ul", {}, ...list.map((it) =>
          el("li", {}, el("span", {}, it.name),
            it.stock > 0 ? el("span", {}, it.stock + " in stock") : el("span", { class: "out" }, "Out of stock"))))),
    );
  }
  return sections;
}

async function loadMenu() {
  const menu = await api.get("/api/menu");
  if (!requireLogin(menu, "/")) return;
  const banner = document.getElementById("banner");
  banner.className = "banner" + (menu.open ? "" : " banner--closed");
  banner.textContent = menu.open
    ? "Reserve before " + menu.cutoff + ". You can cancel until " + menu.cancelDeadline +
      ". Food stays fresh for about " + menu.freshness + " after it is ready. (Server time " + menu.now + ")"
    : "Reservations closed at " + menu.cutoff + " today. (Server time " + menu.now + ")";

  const fresh = menu.items.filter((i) => i.kind === "fresh");
  document.getElementById("fresh").replaceChildren(...fresh.map((i) => freshCard(i, menu)));
  document.getElementById("packaged").replaceChildren(
    ...packagedList(menu.items.filter((i) => i.kind === "packaged")));
}

wireLogout("logout", "/");
loadProfile();
loadMenu();
