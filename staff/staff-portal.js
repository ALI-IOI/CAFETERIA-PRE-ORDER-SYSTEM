"use strict";

let overview = null;
let shownStatus = {};  // reservation code -> status at the last draw, so a change can fade in

// The server writes changes as "8 -> 5"; the page says "8 to 5" (no arrows,
// and an arrow can break across two lines between "-" and ">").
function plain(text) {
  return String(text).replace(/\s*->\s*/g, " to ");
}

function report(r) {
  let text = plain(r.ok ? r.message : r.error);
  if (r.ok && r.warning) text += " " + r.warning;
  say("msg", text, r.ok);
  return r.ok;
}

// Sends one staff action and redraws. The button that sent it stays locked
// (with its busy label) until the server answers.
async function action(path, fields, button, busyLabel) {
  if (button && button.getAttribute("aria-busy") === "true") return;
  const done = button ? busy(button, busyLabel || "Saving…") : () => {};
  let r;
  try {
    r = await api.post(path, fields);
  } catch (e) {
    r = { ok: false, error: "Could not reach the server. Nothing was changed. Try again." };
  }
  done();
  if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html")) return;
  report(r);
  await refresh();
}

// One panel per fresh dish: its cooking queue (first in, first out), reserved
// out of the daily limit, and the two actions.
function freshPanel(item) {
  const pct = item.limit > 0 ? Math.min(100, Math.round((item.reserved / item.limit) * 100)) : 0;
  const meter = el("div", { class: "meter", role: "img",
    "aria-label": item.reserved + " of " + item.limit + " portions reserved" }, el("span"));
  meter.firstChild.style.width = pct + "%";  // CSSOM, allowed by the CSP

  const waiting = item.waitingPortions;
  const queueText = waiting > 0
    ? waiting + (waiting === 1 ? " portion" : " portions") + " waiting, " + item.queueEntries +
      (item.queueEntries === 1 ? " order" : " orders") + " in the cooking queue (first in, first out)."
    : item.reserved > 0 ? "Everything reserved so far is ready." : "Nothing reserved yet.";

  const limitId = "limit-" + item.index, portionsId = "portions-" + item.index;
  const limitButton = el("button", { id: "limit-btn-" + item.index, class: "btn", type: "submit" }, "Set limit");
  const limitForm = el("form", { class: "row" },
    el("div", { class: "field" },
      el("label", { for: limitId }, "Daily limit"),
      el("input", { id: limitId, type: "number", min: item.reserved, max: 10000, value: item.limit, required: true })),
    limitButton);
  limitForm.addEventListener("submit", (e) => {
    e.preventDefault();
    action("/api/staff/limit", { item: item.index, value: document.getElementById(limitId).value }, limitButton);
  });

  const readyButton = el("button", { id: "ready-" + item.index, class: "btn btn--primary", type: "submit",
    disabled: waiting === 0 }, "Mark ready");
  const readyForm = el("form", { class: "row" },
    el("div", { class: "field" },
      el("label", { for: portionsId }, "Portions cooked"),
      // the hint sits above the box, so the button lines up with the box itself
      el("span", { class: "hint", id: portionsId + "-hint" }, "Empty serves everyone waiting."),
      el("input", { id: portionsId, type: "number", min: 1, max: 10000, placeholder: String(waiting),
        "aria-describedby": portionsId + "-hint" })),
    readyButton);
  readyForm.addEventListener("submit", (e) => {
    e.preventDefault();
    action("/api/staff/ready", { item: item.index, portions: document.getElementById(portionsId).value || "0" },
      readyButton, "Marking…");
  });

  return el("article", { class: "kpi" + (waiting === 0 ? " kpi--idle" : "") },
    el("h3", {}, el("span", {}, item.name), el("span", { class: "figure" }, item.remaining, el("span", { class: "visually-hidden" }, " left"))),
    el("p", { class: "food__meta" }, item.remaining + " left of " + item.limit + ", " + item.reserved + " reserved." +
      (item.waitlist > 0 ? " " + item.waitlist + " on the waitlist; raising the limit serves them first." : "")),
    meter,
    el("p", { class: "dish__flag" }, queueText),
    el("p", { class: "food__meta" }, "Cooking queue: " + item.queueEntries + " of " + item.queueCapacity +
      " places used, first in, first out."),
    readyForm,
    limitForm);
}

// One row of the "Menu items" table: picture, price, availability, upload.
function itemRow(item) {
  const priceId = "price-" + item.index, availId = "avail-" + item.index, fileId = "file-" + item.index;
  const priceButton = el("button", { class: "btn", type: "submit" }, "Save");
  const priceForm = el("form", { class: "row" },
    el("div", { class: "field" },
      el("label", { for: priceId, class: "visually-hidden" }, "Price of " + item.name + " in taka"),
      el("input", { id: priceId, type: "number", min: 0, max: 9999, value: item.price, required: true })),
    priceButton);
  priceForm.addEventListener("submit", (e) => {
    e.preventDefault();
    action("/api/staff/items/price", { item: item.index, price: document.getElementById(priceId).value }, priceButton);
  });

  const avail = el("input", { id: availId, type: "checkbox", checked: item.available });
  avail.addEventListener("change", () =>
    action("/api/staff/items/available", { item: item.index, available: avail.checked ? "1" : "0" }));

  const whoId = "who-" + item.index;
  let who = el("span", { class: "hint" }, "Everyone");
  if (item.kind === "fresh") {
    who = el("select", { id: whoId, class: "who-select", "aria-label": "Who can order " + item.name },
      el("option", { value: "0", selected: !item.teachersOnly }, "Everyone"),
      el("option", { value: "1", selected: item.teachersOnly }, "Teachers only"));
    who.addEventListener("change", () =>
      action("/api/staff/items/teachers", { item: item.index, teachersOnly: who.value }));
  }

  const fileInput = el("input", { id: fileId, type: "file", accept: "image/jpeg,image/png,image/webp" });
  const uploadButton = el("button", { class: "btn", type: "submit" }, "Upload");
  const uploadForm = el("form", { class: "row" },
    el("div", { class: "field" }, el("label", { for: fileId, class: "visually-hidden" }, "New picture for " + item.name), fileInput),
    uploadButton);
  uploadForm.addEventListener("submit", async (e) => {
    e.preventDefault();
    if (!fileInput.files.length) {
      say("msg", "Choose a picture first.", false);
      return;
    }
    const data = new FormData();
    data.append("item", String(item.index));
    data.append("image", fileInput.files[0]);
    const done = busy(uploadButton, "Uploading…");
    const r = await api.upload("/api/staff/items/image", data);
    done();
    if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html")) return;
    report(r);
    refresh();
  });

  return el("tr", {},
    el("td", {}, el("img", { class: "thumb", src: item.image, alt: "", width: 64, height: 44 })),
    el("th", { scope: "row" }, item.name, el("span", { class: "hint" }, item.category + ", " + (item.kind === "fresh" ? "cooked to order" : "packaged")),
      item.teachersOnly ? el("span", { class: "special-tag" }, "Teachers’ special") : ""),
    el("td", {}, priceForm),
    el("td", {}, el("label", { for: availId, class: "check" }, el("span", { class: "visually-hidden" }, item.name + " available today"), avail)),
    el("td", {}, who),
    el("td", {}, uploadForm),
    el("td", {}, el("button", {
      class: "btn btn--danger", type: "button", "aria-label": "Remove " + item.name + " from the menu",
      onclick: (e) => action("/api/staff/items/remove", { item: item.index }, e.currentTarget, "Removing…"),
    }, "Remove")));
}

function stockRow(item) {
  const id = "stock-" + item.index;
  const button = el("button", { class: "btn", type: "submit" }, "Save");
  const form = el("form", { class: "row" },
    el("div", { class: "field" },
      el("label", { for: id, class: "visually-hidden" }, "Stock for " + item.name),
      el("input", { id, type: "number", min: 0, max: 9999, value: item.stock, required: true })),
    button);
  form.addEventListener("submit", (e) => {
    e.preventDefault();
    action("/api/staff/stock", { item: item.index, value: document.getElementById(id).value }, button);
  });
  return el("tr", {}, el("th", { scope: "row" }, item.name), el("td", {}, item.category), el("td", {}, form));
}

function reservationRow(res, before) {
  const collect = res.status === "Ready"
    ? el("button", { class: "btn", type: "button", "aria-label": "Mark " + res.code + " collected",
        onclick: (e) => action("/api/staff/collected", { id: res.id }, e.currentTarget, "Saving…") }, "Collected")
    : "";
  const changed = before[res.code] !== undefined && before[res.code] !== res.status;
  const classes = [res.status === "Ready" ? "is-ready" : "", changed ? "is-changed" : ""].filter(Boolean).join(" ");
  return el("tr", { class: classes || null },
    el("th", { scope: "row" }, res.code),
    el("td", {}, res.item),
    el("td", { class: "num" }, res.quantity),
    el("td", {}, res.user),
    el("td", {}, res.reservedAt),
    el("td", {}, statusChip(res.status), res.status === "Ready" ? " until " + res.freshUntil : ""),
    el("td", {}, collect));
}

async function loadReservations(q) {
  let r;
  try {
    r = await api.get("/api/staff/reservations" + (q ? "?q=" + encodeURIComponent(q) : ""));
  } catch (e) {
    r = { ok: false, error: "Couldn’t load the reservations. Try again." };
  }
  if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html")) return;
  const state = document.getElementById("res-state");
  if (!r.ok) {
    listState(state, "error", (r.status === 0 ? "Couldn’t load the reservations. " : r.error + " ") + "Try again.",
      () => loadReservations(q));
    return;
  }
  listState(state, null);
  document.getElementById("res-caption").textContent =
    q ? "Search result for " + q : "Sorted by item, then reservation time";
  const before = shownStatus;
  shownStatus = {};
  for (const res of r.reservations) shownStatus[res.code] = res.status;
  document.getElementById("res-rows").replaceChildren(...r.reservations.map((res) => reservationRow(res, before)));
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

// The undo history is the staff action stack: newest on top, and Undo pops it.
function renderHistory(o) {
  document.getElementById("stack-size").textContent =
    o.historySize === 0 ? "Nothing to undo." : o.historySize + " of " + o.historyCapacity + " places on the stack used, newest on top.";
  const undo = document.getElementById("undo");
  if (undo.getAttribute("aria-busy") !== "true") undo.disabled = o.historySize === 0;
  document.getElementById("history").replaceChildren(...o.history.map((a, i) =>
    el("li", {}, i === 0 ? el("span", { class: "top" }, "Undo reverses this") : null,
      plain(a.description), el("span", { class: "when" }, a.at + " by " + a.staff))));
}

// Token counter: the tokens whose food is all ready, first ready first, each with
// its owner and a hand-over button; tokens still cooking are counted below the form.
async function loadTokens() {
  const box = document.getElementById("tokens-state");
  let r;
  try {
    r = await api.get("/api/staff/tokens");
  } catch (e) {
    r = { ok: false, status: 0 };
  }
  if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html")) return;
  if (!r.ok || !Array.isArray(r.tokens)) {
    listState(box, "error", "Couldn’t load today’s tokens. Try again.", refresh);
    return;
  }
  listState(box, null);
  const ready = r.tokens.filter((t) => t.state === "ready")
    .sort((a, b) => (a.readyAt < b.readyAt ? -1 : a.readyAt > b.readyAt ? 1 : a.token - b.token));
  const cooking = r.tokens.filter((t) => t.state === "cooking");
  document.getElementById("tokens-summary").textContent =
    ready.length + (ready.length === 1 ? " token" : " tokens") + " ready at the counter, " +
    cooking.length + " still cooking, " + r.tokens.length + " issued today.";
  document.getElementById("ready-tokens").replaceChildren(...ready.map((t) => el("li", {},
    tokenStamp("Ready", t.text, "", "ready", "small"),
    el("div", {},
      el("strong", {}, t.ready + (t.ready === 1 ? " dish" : " dishes")),
      el("p", {}, t.user)),
    el("button", { class: "btn", type: "button", "aria-label": "Hand over token " + t.text,
      onclick: (e) => action("/api/staff/tokens/handover", { token: t.token }, e.currentTarget, "Saving…") }, "Hand over"))));
}

// The boxes above the staff tables carry their loading and error states.
const LIST_STATES = ["items-state", "stock-state", "res-state"];

async function refresh() {
  const restoreFocus = keepFocus();
  const fresh = document.getElementById("fresh");
  if (!overview) {
    listState(fresh, "loading");
    for (const id of LIST_STATES) listState(document.getElementById(id), "loading");
  }
  let o;
  try {
    o = await api.get("/api/staff/overview");
  } catch (e) {
    o = { ok: false, status: 0 };
  }
  if (!requireLogin(o, "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html")) return;
  if (!o.ok || !Array.isArray(o.items)) {
    listState(fresh, "error", "Couldn’t load the kitchen overview. Check the connection to the server and try again.", refresh);
    for (const id of LIST_STATES) {
      listState(document.getElementById(id), "error", "Couldn’t load this list. Try again.", refresh);
    }
    document.getElementById("stack-size").textContent = "Couldn’t load the undo history.";
    document.getElementById("undo").disabled = true;
    return;
  }
  for (const id of ["items-state", "stock-state"]) listState(document.getElementById(id), null);
  overview = o;
  document.getElementById("who").textContent = "Signed in: " + o.staffName;
  document.getElementById("clock").textContent =
    "Server time " + o.now + ". Reservations close at " + o.settings.cutoff + ". " +
    o.reminderQueue + (o.reminderQueue === 1 ? " ready order" : " ready orders") + " waiting for collection.";
  const freshItems = o.items.filter((i) => i.kind === "fresh");
  if (freshItems.length) fresh.replaceChildren(...freshItems.map(freshPanel));
  else listState(fresh, "empty", "No fresh dishes on the menu. Add one under Menu items.");
  document.getElementById("item-rows").replaceChildren(...o.items.map(itemRow));
  document.getElementById("ni-cats").replaceChildren(
    ...[...new Set(o.items.map((i) => i.category))].map((c) => el("option", { value: c })));
  document.getElementById("stock-rows").replaceChildren(
    ...o.items.filter((i) => i.kind === "packaged").map(stockRow));
  fillSettings(o.settings);
  renderHistory(o);
  document.getElementById("email-mode").textContent = (o.emailMode === "outbox"
    ? "Email sending is not set up yet: emails are saved as files on the server instead."
    : "Emails are sent through the cafeteria’s SMTP account.") +
    (o.emailsWaiting > 0
      ? " " + o.emailsWaiting + (o.emailsWaiting === 1 ? " email" : " emails") + " could not be sent and will be tried again after 1, 5 and 15 minutes."
      : " No emails waiting to retry.");
  await loadReservations(document.getElementById("q").value.trim());
  await loadTokens();
  await loadOrders();
  await loadCoupons();
  restoreFocus();
}

// The section list marks the section on screen.
function watchSections() {
  const links = [...document.querySelectorAll(".portal__nav a")];
  const targets = links.map((a) => document.querySelector(a.getAttribute("href"))).filter(Boolean);
  const mark = (id) => links.forEach((a) => {
    if (a.getAttribute("href") === "#" + id) a.setAttribute("aria-current", "true");
    else a.removeAttribute("aria-current");
  });
  if (!("IntersectionObserver" in window)) return;
  const seen = new IntersectionObserver((entries) => {
    const top = entries.filter((e) => e.isIntersecting).sort((a, b) => a.boundingClientRect.top - b.boundingClientRect.top)[0];
    if (top) mark(top.target.id);
  }, { rootMargin: "0px 0px -60% 0px" });
  targets.forEach((t) => seen.observe(t));
}

document.getElementById("search").addEventListener("submit", (e) => {
  e.preventDefault();
  loadReservations(document.getElementById("q").value.trim());
});
document.getElementById("clear").addEventListener("click", () => {
  document.getElementById("q").value = "";
  loadReservations("");
});
document.getElementById("undo").addEventListener("click", (e) => action("/api/staff/undo", {}, e.currentTarget, "Undoing…"));
document.getElementById("handover").addEventListener("submit", async (e) => {
  e.preventDefault();
  const form = e.target;
  if (!checkFields(form)) return;
  const input = document.getElementById("tok");
  await action("/api/staff/tokens/handover", { token: input.value.trim() }, form.querySelector("button"), "Saving…");
  input.value = "";
});

document.getElementById("settings-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const form = e.target;
  if (!checkFields(form)) return;
  if (!overview) {
    say("msg", "The settings are still loading. Try again in a moment.", false);
    return;
  }
  const s = overview.settings;
  const changes = [];
  for (const key of ["cutoff", "cancel_deadline", "freshness_minutes", "reminder_minutes", "per_user_limit"]) {
    if (String(form[key].value) !== String(s[key])) changes.push([key, form[key].value]);
  }
  if (changes.length === 0) {
    say("msg", "Nothing changed.");
    return;
  }
  const done = busy(form.querySelector("button[type=submit]"), "Saving…");
  // Each change is one undoable action on the stack.
  for (const [key, value] of changes) {
    const r = await api.post("/api/staff/settings", { key, value });
    if (!report(r)) break;
  }
  done();
  await refresh();
});

document.getElementById("add-item").addEventListener("submit", async (e) => {
  e.preventDefault();
  if (!checkFields(e.target)) return;
  const done = busy(e.target.querySelector("button[type=submit]"), "Adding…");
  const r = await api.post("/api/staff/items", {
    name: document.getElementById("ni-name").value.trim(),
    category: document.getElementById("ni-cat").value.trim(),
    kind: document.getElementById("ni-kind").value,
    price: document.getElementById("ni-price").value,
    amount: document.getElementById("ni-amount").value,
    teachers: document.getElementById("ni-who").value,
  });
  done();
  if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html")) return;
  if (report(r)) e.target.reset();
  refresh();
});

wireLogout("logout", "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html");
refresh();
watchSections();

// ---- orders placed --------------------------------------------------------

function orderRow(o) {
  const dishes = el("ul", { class: "order-dishes" }, ...o.dishes.map((d) => el("li", {},
    el("span", {}, d.item + " × " + d.quantity),
    el("span", { class: "hint" }, d.code + ", " + taka(d.price) + " each"),
    statusChip(d.status))));
  const pay = o.cancelled
    ? el("span", { class: "hint" }, "Cancelled")
    : el("div", {}, el("strong", { class: "num" }, "৳ " + o.toPay),
        o.discount > 0 ? el("span", { class: "hint" }, "৳ " + o.subtotal + " less ৳ " + o.discount + " (coupon " + o.coupon + ")") : "");
  return el("tr", { class: o.cancelled ? "is-muted" : null },
    el("th", { scope: "row" }, el("span", { class: "visually-hidden" }, "Token "), el("span", { class: "order-token" }, o.tokenText)),
    el("td", {}, el("strong", {}, o.name || o.user),
      el("span", { class: "hint" }, (o.role ? roleLabel(o.role) + ", " : "") + o.user),
      el("span", { class: "hint" }, "Placed " + o.placedAt)),
    el("td", {}, dishes),
    el("td", {}, pay));
}

async function loadOrders() {
  const state = document.getElementById("orders-state");
  let r;
  try {
    r = await api.get("/api/staff/orders");
  } catch (e) {
    r = { ok: false, status: 0 };
  }
  if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html")) return;
  if (!r.ok || !Array.isArray(r.orders)) {
    listState(state, "error", "Couldn’t load today’s orders. Try again.", refresh);
    return;
  }
  listState(state, null);
  const live = r.orders.filter((o) => !o.cancelled);
  const due = live.reduce((sum, o) => sum + o.toPay, 0);
  document.getElementById("orders-summary").textContent = r.orders.length === 0 ? "" :
    live.length + (live.length === 1 ? " order" : " orders") + " today, ৳ " + due + " to collect at the counter.";
  document.getElementById("order-rows").replaceChildren(...r.orders.map(orderRow));
  document.getElementById("orders-empty").hidden = r.orders.length > 0;
}

// ---- coupons --------------------------------------------------------------

function couponRow(c) {
  const activeId = "coupon-on-" + c.code;
  const on = el("input", { id: activeId, type: "checkbox", checked: c.active });
  on.addEventListener("change", () =>
    action("/api/staff/coupons/active", { code: c.code, active: on.checked ? "1" : "0" }));
  const days = c.from && c.to ? c.from + " to " + c.to : c.from ? "From " + c.from : c.to ? "Until " + c.to : "Every day";
  return el("tr", { class: c.state === "Working today" ? null : "is-muted" },
    el("th", { scope: "row" }, c.code, el("span", { class: "hint" }, c.occasion)),
    el("td", {}, c.text),
    el("td", {}, c.audienceText),
    el("td", {}, days, el("span", { class: "hint" }, c.state)),
    el("td", { class: "num" }, c.uses + (c.maxUses > 0 ? " of " + c.maxUses : "")),
    el("td", {}, el("label", { for: activeId, class: "check" },
      el("span", { class: "visually-hidden" }, "Coupon " + c.code + " switched on"), on)),
    el("td", {}, el("button", {
      class: "btn btn--danger", type: "button", "aria-label": "Remove coupon " + c.code,
      onclick: (e) => action("/api/staff/coupons/remove", { code: c.code }, e.currentTarget, "Removing…"),
    }, "Remove")));
}

async function loadCoupons() {
  const state = document.getElementById("coupons-state");
  let r;
  try {
    r = await api.get("/api/staff/coupons");
  } catch (e) {
    r = { ok: false, status: 0 };
  }
  if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html")) return;
  if (!r.ok || !Array.isArray(r.coupons)) {
    listState(state, "error", "Couldn’t load the coupons. Try again.", refresh);
    return;
  }
  listState(state, null);
  document.getElementById("coupon-rows").replaceChildren(...r.coupons.map(couponRow));
  document.getElementById("coupons-empty").hidden = r.coupons.length > 0;
}

document.getElementById("add-coupon").addEventListener("submit", async (e) => {
  e.preventDefault();
  if (!checkFields(e.target)) return;
  const done = busy(e.target.querySelector("button[type=submit]"), "Making…");
  let r;
  try {
    r = await api.post("/api/staff/coupons", {
      code: document.getElementById("cp-code").value.trim(),
      occasion: document.getElementById("cp-occasion").value.trim(),
      kind: document.getElementById("cp-kind").value,
      value: document.getElementById("cp-value").value,
      audience: document.getElementById("cp-audience").value,
      from: document.getElementById("cp-from").value,
      to: document.getElementById("cp-to").value,
      maxUses: document.getElementById("cp-max").value,
    });
  } catch (err) {
    r = { ok: false, error: "Could not reach the server. Nothing was changed. Try again." };
  }
  done();
  if (!requireLogin(r, "/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html")) return;
  if (report(r)) e.target.reset();
  refresh();
});
