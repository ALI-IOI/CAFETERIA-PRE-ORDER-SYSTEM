// Shared helpers for every page. All text from the server is inserted with
// textContent (never innerHTML), so names and messages cannot inject HTML.
"use strict";

const api = {
  async get(path) {
    return api._send(path, { credentials: "same-origin" });
  },
  async post(path, fields) {
    return api._send(path, {
      method: "POST",
      credentials: "same-origin",
      headers: { "X-Cafe": "1", "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams(fields || {}),
    });
  },
  // Multipart upload (staff pictures). The browser sets the multipart boundary.
  async upload(path, formData) {
    return api._send(path, {
      method: "POST",
      credentials: "same-origin",
      headers: { "X-Cafe": "1" },
      body: formData,
    });
  },
  // A request that never reached the server comes back as an ordinary error
  // answer, so no caller is left with a locked button or a page stuck loading.
  async _send(path, options) {
    let res;
    try {
      res = await fetch(path, options);
    } catch (e) {
      return { ok: false, status: 0, error: "Could not reach the cafeteria. Check your connection and try again." };
    }
    return api._handle(res);
  },
  async _handle(res) {
    let data;
    try {
      data = await res.json();
    } catch (e) {
      data = { ok: false, error: "Unexpected server response (" + res.status + ")." };
    }
    data.status = res.status;
    return data;
  },
};

// Builds an element: el("li", { class: "x" }, "text", childNode, ...)
function el(tag, attrs, ...children) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v === false || v === null || v === undefined) continue;
    if (k === "class") node.className = v;
    else if (k.startsWith("on")) node.addEventListener(k.slice(2), v);
    else node.setAttribute(k, v === true ? "" : v);
  }
  for (const c of children) {
    if (c === null || c === undefined || c === false) continue;
    node.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return node;
}

// Shows a message in a role="status" element: ok true = success, false = error,
// left out = information. It fades in each time (CSS .is-new).
function say(target, text, ok) {
  const node = typeof target === "string" ? document.getElementById(target) : target;
  node.textContent = text || "";
  node.className = "msg " + (ok === true ? "msg--ok" : ok === false ? "msg--error" : "msg--info");
  void node.offsetWidth;  // restart the fade when the same message comes again
  if (text) node.classList.add("is-new");
}

// Loading, empty and error states for a list: replaces the container's content
// with one .state paragraph (an error adds "Try again"); kind null removes it.
function listState(container, kind, text, onRetry) {
  if (kind === null) {
    for (const s of container.querySelectorAll(":scope > .state")) s.remove();
    return;
  }
  const box = el("div", { class: "state state--" + kind + " is-new", role: kind === "error" ? "alert" : "status" },
    el("p", {}, text || (kind === "loading" ? "Loading…" : "")));
  if (kind === "error" && onRetry) {
    // the button is replaced by what loads, so focus moves to the list itself
    box.append(el("button", { class: "btn", type: "button", onclick: () => {
      container.setAttribute("tabindex", "-1");
      container.focus();
      onRetry();
    } }, "Try again"));
  }
  container.replaceChildren(box);
}

// Shows (or clears, with "") the error under one field: red edge, the message
// in a .field__error linked to the input by aria-describedby.
function fieldError(input, text) {
  const field = input.closest(".field");
  const id = input.id + "-error";
  let note = document.getElementById(id);
  if (!note) {
    note = el("span", { class: "field__error", id });
    input.after(note);
  }
  note.textContent = text || "";
  if (field) field.classList.toggle("field--invalid", Boolean(text));
  if (text) input.setAttribute("aria-invalid", "true");
  else input.removeAttribute("aria-invalid");
  const ids = (input.getAttribute("aria-describedby") || "").split(" ").filter((x) => x && x !== id);
  if (text) ids.push(id);
  if (ids.length) input.setAttribute("aria-describedby", ids.join(" "));
  else input.removeAttribute("aria-describedby");
}

// Checks a form's required, length and pattern rules in the browser before it is
// sent. Each field's message is its data-error attribute. Returns true when all
// are valid; otherwise shows every error and moves focus to the first.
function checkFields(form) {
  let first = null;
  for (const input of form.querySelectorAll("input[data-error], select[data-error]")) {
    const value = input.value.trim();
    const bad = !input.disabled && (!input.checkValidity() || (input.required && value === ""));
    fieldError(input, bad ? input.dataset.error : "");
    if (bad && !first) first = input;
  }
  if (first) first.focus();
  return first === null;
}

// Locks a button while the server answers: busy(button, "Reserving…") returns a
// function that puts it back. A second call while it is busy changes nothing.
function busy(button, label) {
  if (button.getAttribute("aria-busy") === "true") return () => {};
  const before = { text: button.textContent, disabled: button.disabled };
  button.disabled = true;
  button.setAttribute("aria-busy", "true");
  button.classList.add("is-busy");
  button.textContent = label;
  return () => {
    button.textContent = before.text;
    button.disabled = before.disabled;
    button.removeAttribute("aria-busy");
    button.classList.remove("is-busy");
  };
}

// "৳ 120", or a note when staff have not set a price yet.
function taka(amount) {
  return amount > 0 ? "৳ " + amount : "Price at counter";  // no line break between sign and number
}

// sessionStorage can be unavailable (private mode, blocked storage).
const store = {
  get(key, fallback) {
    try {
      const v = sessionStorage.getItem(key);
      return v === null ? fallback : JSON.parse(v);
    } catch (e) {
      return fallback;
    }
  },
  set(key, value) {
    try {
      sessionStorage.setItem(key, JSON.stringify(value));
    } catch (e) {
      /* the tray then lasts only until the page is reloaded */
    }
  },
};

// Phones show table rows as cards (CSS, max-width 640px); each cell then needs its
// column name, copied here from the table's header row whenever rows change.
function labelTableCells(table) {
  const heads = [...table.querySelectorAll("thead th")].map((th) =>
    th.querySelector(".visually-hidden") ? "" : th.textContent.trim());
  for (const row of table.querySelectorAll("tbody tr")) {
    [...row.children].forEach((cell, i) => {
      if (heads[i]) cell.setAttribute("data-label", heads[i]);
    });
  }
}
document.addEventListener("DOMContentLoaded", () => {
  for (const table of document.querySelectorAll(".table-wrap table")) {
    labelTableCells(table);
    new MutationObserver(() => labelTableCells(table)).observe(table, { childList: true, subtree: true });
  }
});

// The green bar at the top of the public and student pages.
async function fillAnnouncement(id) {
  const bar = document.getElementById(id || "announce");
  if (!bar) return;
  const info = await api.get("/api/public/info");
  if (!info.ok) return;
  bar.textContent = info.open
    ? "Orders are open until " + info.cutoff + ". You pay at the counter when you collect."
    : "Orders are closed for today. You can order again tomorrow morning.";
}

// "student" / "teacher" / "employee" as people read it.
function roleLabel(role) {
  return role === "teacher" ? "Teacher" : role === "employee" ? "University staff" : "Student";
}

function statusChip(status) {
  return el("span", { class: "chip chip--" + status }, status);
}

// Sends people who are not logged in back to the right login page.
function requireLogin(result, loginPage) {
  if (result.status === 401 || result.status === 403) {
    window.location.href = loginPage;
    return false;
  }
  return true;
}

function wireLogout(buttonId, after) {
  const b = document.getElementById(buttonId);
  if (!b) return;
  b.addEventListener("click", async () => {
    await api.post("/api/logout");
    window.location.href = after;
  });
}
