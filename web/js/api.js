// Shared helpers for every page. All text from the server is inserted with
// textContent (never innerHTML), so names and messages cannot inject HTML.
"use strict";

const api = {
  async get(path) {
    return api._handle(await fetch(path, { credentials: "same-origin" }));
  },
  async post(path, fields) {
    const body = new URLSearchParams(fields || {});
    const res = await fetch(path, {
      method: "POST",
      credentials: "same-origin",
      headers: { "X-Cafe": "1", "Content-Type": "application/x-www-form-urlencoded" },
      body,
    });
    return api._handle(res);
  },
  // Multipart upload (staff pictures). The browser sets the multipart boundary.
  async upload(path, formData) {
    const res = await fetch(path, {
      method: "POST",
      credentials: "same-origin",
      headers: { "X-Cafe": "1" },
      body: formData,
    });
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

// Shows a message in a role="status" element.
function say(target, text, ok) {
  const node = typeof target === "string" ? document.getElementById(target) : target;
  node.textContent = text || "";
  node.className = "msg " + (ok ? "msg--ok" : "msg--error");
}

// "৳ 120", or a note when staff have not set a price yet.
function taka(amount) {
  return amount > 0 ? "৳ " + amount : "Price at counter";
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
