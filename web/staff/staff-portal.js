"use strict";

let overview = null;

function report(r) {
  let text = r.ok ? r.message : r.error;
  if (r.ok && r.warning) text += " " + r.warning;
  say("msg", text, r.ok);
  return r.ok;
}

async function action(path, fields) {
  const r = await api.post(path, fields);
  if (!requireLogin(r, "/staff/login.html")) return;
  report(r);
  await refresh();
}

function freshPanel(item) {
  const pct = item.limit > 0 ? Math.min(100, Math.round((item.reserved / item.limit) * 100)) : 0;
  const meter = el("div", { class: "meter", role: "img",
    "aria-label": item.reserved + " of " + item.limit + " portions reserved" }, el("span"));
  meter.firstChild.style.width = pct + "%";  // CSSOM, allowed by the CSP

  const limitId = "limit-" + item.index, portionsId = "portions-" + item.index;
  const limitForm = el("form", { class: "row" },
    el("div", { class: "field" },
      el("label", { for: limitId }, "Daily limit"),
      el("input", { id: limitId, type: "number", min: item.reserved, max: 10000, value: item.limit, required: true })),
    el("button", { class: "btn btn--ghost", type: "submit" }, "Set limit"));
  limitForm.addEventListener("submit", (e) => {
    e.preventDefault();
    action("/api/staff/limit", { item: item.index, value: document.getElementById(limitId).value });
  });

  const readyForm = el("form", { class: "row" },
    el("div", { class: "field" },
      el("label", { for: portionsId }, "Portions cooked", el("span", { class: "hint" }, "empty = all waiting")),
      el("input", { id: portionsId, type: "number", min: 1, max: 10000, placeholder: String(item.waitingPortions) })),
    el("button", { class: "btn", type: "submit", disabled: item.waitingPortions === 0 }, "Mark ready"));
  readyForm.addEventListener("submit", (e) => {
    e.preventDefault();
    action("/api/staff/ready", { item: item.index, portions: document.getElementById(portionsId).value || "0" });
  });

  return el("article", { class: "card kpi" },
    el("h3", {}, el("span", {}, item.name), el("span", { class: "hint" }, item.remaining + " left")),
    meter,
    el("p", { class: "food__meta" },
      item.reserved + " of " + item.limit + " reserved · " + item.waitingPortions +
      " portion(s) waiting to cook · queue " + item.queueEntries + "/" + item.queueCapacity +
      (item.waitlist > 0 ? " · " + item.waitlist + " on the waitlist (raising the limit serves them first)" : "")),
    readyForm,
    limitForm);
}

// One row of the "Menu items" table: picture, price, availability, upload.
function itemRow(item) {
  const priceId = "price-" + item.index, availId = "avail-" + item.index, fileId = "file-" + item.index;
  const priceForm = el("form", { class: "row" },
    el("div", { class: "field" },
      el("label", { for: priceId, class: "visually-hidden" }, "Price of " + item.name + " in taka"),
      el("input", { id: priceId, type: "number", min: 0, max: 9999, value: item.price, required: true })),
    el("button", { class: "btn btn--ghost", type: "submit" }, "Save"));
  priceForm.addEventListener("submit", (e) => {
    e.preventDefault();
    action("/api/staff/items/price", { item: item.index, price: document.getElementById(priceId).value });
  });

  const avail = el("input", { id: availId, type: "checkbox", checked: item.available });
  avail.addEventListener("change", () =>
    action("/api/staff/items/available", { item: item.index, available: avail.checked ? "1" : "0" }));

  const fileInput = el("input", { id: fileId, type: "file", accept: "image/jpeg,image/png,image/webp" });
  const uploadForm = el("form", { class: "row" },
    el("div", { class: "field" }, el("label", { for: fileId, class: "visually-hidden" }, "New picture for " + item.name), fileInput),
    el("button", { class: "btn btn--ghost", type: "submit" }, "Upload"));
  uploadForm.addEventListener("submit", async (e) => {
    e.preventDefault();
    if (!fileInput.files.length) {
      say("msg", "Choose a picture first.");
      return;
    }
    const data = new FormData();
    data.append("item", String(item.index));
    data.append("image", fileInput.files[0]);
    const r = await api.upload("/api/staff/items/image", data);
    if (!requireLogin(r, "/staff/login.html")) return;
    report(r);
    refresh();
  });

  return el("tr", {},
    el("td", {}, el("img", { class: "thumb", src: item.image, alt: "" })),
    el("th", { scope: "row" }, item.name, el("br"), el("span", { class: "hint" }, item.category + " · " + item.kind)),
    el("td", {}, priceForm),
    el("td", {}, el("label", { for: availId, class: "visually-hidden" }, item.name + " available today"), avail),
    el("td", {}, uploadForm));
}

function stockRow(item) {
  const id = "stock-" + item.index;
  const form = el("form", { class: "row" },
    el("div", { class: "field" },
      el("label", { for: id, class: "visually-hidden" }, "Stock for " + item.name),
      el("input", { id, type: "number", min: 0, max: 9999, value: item.stock, required: true })),
    el("button", { class: "btn btn--ghost", type: "submit" }, "Save"));
  form.addEventListener("submit", (e) => {
    e.preventDefault();
    action("/api/staff/stock", { item: item.index, value: document.getElementById(id).value });
  });
  return el("tr", {}, el("th", { scope: "row" }, item.name), el("td", {}, item.category), el("td", {}, form));
}

function reservationRow(res) {
  const collect = res.status === "Ready"
    ? el("button", { class: "btn btn--ghost", type: "button", "aria-label": "Mark " + res.code + " collected",
        onclick: () => action("/api/staff/collected", { id: res.id }) }, "Collected")
    : "";
  return el("tr", {},
    el("th", { scope: "row" }, res.code),
    el("td", {}, res.item),
    el("td", { class: "num" }, res.quantity),
    el("td", {}, res.user),
    el("td", {}, res.reservedAt),
    el("td", {}, statusChip(res.status), res.status === "Ready" ? " until " + res.freshUntil : ""),
    el("td", {}, collect));
}

async function loadReservations(q) {
  const r = await api.get("/api/staff/reservations" + (q ? "?q=" + encodeURIComponent(q) : ""));
  if (!requireLogin(r, "/staff/login.html")) return;
  if (!r.ok) {
    report(r);
    return;
  }
  document.getElementById("res-caption").textContent =
    q ? "Search result for " + q : "Sorted by item, then reservation time";
  document.getElementById("res-rows").replaceChildren(...r.reservations.map(reservationRow));
  document.getElementById("res-empty").hidden = r.reservations.length > 0;
}

function fillSettings(s) {
  const form = document.getElementById("settings-form");
  form.cutoff.value = s.cutoff;
  form.cancel_deadline.value = s.cancel_deadline;
  form.freshness_minutes.value = s.freshness_minutes;
  form.reminder_minutes.value = s.reminder_minutes;
  form.per_user_limit.value = s.per_user_limit;
}

async function refresh() {
  const o = await api.get("/api/staff/overview");
  if (!requireLogin(o, "/staff/login.html")) return;
  overview = o;
  document.getElementById("who").textContent = "Signed in: " + o.staffName;
  document.getElementById("clock").textContent =
    "Server time " + o.now + " · reservations close at " + o.settings.cutoff +
    " · " + o.reminderQueue + " ready order(s) waiting for collection";
  const fresh = o.items.filter((i) => i.kind === "fresh");
  document.getElementById("fresh").replaceChildren(...fresh.map(freshPanel));
  document.getElementById("item-rows").replaceChildren(...o.items.map(itemRow));
  document.getElementById("ni-cats").replaceChildren(
    ...[...new Set(o.items.map((i) => i.category))].map((c) => el("option", { value: c })));
  document.getElementById("stock-rows").replaceChildren(
    ...o.items.filter((i) => i.kind === "packaged").map(stockRow));
  fillSettings(o.settings);

  document.getElementById("stack-size").textContent =
    "Undo history: " + o.historySize + " of " + o.historyCapacity + " actions (newest first).";
  document.getElementById("undo").disabled = o.historySize === 0;
  document.getElementById("history").replaceChildren(...o.history.map((a) =>
    el("li", {}, a.description, el("br"), el("span", { class: "when" }, a.at + " · " + a.staff))));
  document.getElementById("email-mode").textContent = (o.emailMode === "outbox"
    ? "SMTP is not configured yet: emails are saved in data/outbox on the server."
    : "Emails are sent through the cafeteria's SMTP account.") +
    (o.emailsWaiting > 0
      ? " " + o.emailsWaiting + " email(s) failed and will be retried automatically (after 1, 5 and 15 minutes)."
      : " No emails waiting to retry.");
  await loadReservations(document.getElementById("q").value.trim());
}

document.getElementById("search").addEventListener("submit", (e) => {
  e.preventDefault();
  loadReservations(document.getElementById("q").value.trim());
});
document.getElementById("clear").addEventListener("click", () => {
  document.getElementById("q").value = "";
  loadReservations("");
});
document.getElementById("undo").addEventListener("click", () => action("/api/staff/undo", {}));

document.getElementById("settings-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const form = e.target;
  const s = overview.settings;
  const changes = [];
  for (const key of ["cutoff", "cancel_deadline", "freshness_minutes", "reminder_minutes", "per_user_limit"]) {
    if (String(form[key].value) !== String(s[key])) changes.push([key, form[key].value]);
  }
  if (changes.length === 0) {
    say("msg", "Nothing changed.", true);
    return;
  }
  // Each change is one undoable action on the stack.
  for (const [key, value] of changes) {
    const r = await api.post("/api/staff/settings", { key, value });
    if (!report(r)) break;
  }
  await refresh();
});

wireLogout("logout", "/staff/login.html");
refresh();

document.getElementById("add-item").addEventListener("submit", async (e) => {
  e.preventDefault();
  const r = await api.post("/api/staff/items", {
    name: document.getElementById("ni-name").value.trim(),
    category: document.getElementById("ni-cat").value.trim(),
    kind: document.getElementById("ni-kind").value,
    price: document.getElementById("ni-price").value,
    amount: document.getElementById("ni-amount").value,
  });
  if (!requireLogin(r, "/staff/login.html")) return;
  if (report(r)) e.target.reset();
  refresh();
});
