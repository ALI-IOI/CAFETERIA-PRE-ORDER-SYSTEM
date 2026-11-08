// Live demo only (GitHub Pages). Loaded before js/api.js on every page.
// GitHub Pages serves files, not programs, so the cafeteria's C++ server runs in
// this browser instead: compiled to WebAssembly, in web-demo/cafe-worker.js.
// This file
//   - answers the pages' /api/ requests with that server (fetch is wrapped),
//   - keeps its session cookies (in localStorage, so every tab shares them),
//   - adds the demo bar: what this is, the cafeteria clock, the email inbox,
//     "+1 hour" and "Reset demo", and the demo accounts on the two login pages.
"use strict";

(() => {
  const BASE = document.currentScript.src.replace(/\/demo\/demo\.js(\?.*)?$/, "").replace(/^https?:\/\/[^/]+/, "");
  const workerUrl = BASE + "/demo/cafe-worker.js";
  let port;
  try {
    const sw = new SharedWorker(workerUrl, { name: "cafe-demo" });
    port = sw.port;
    port.start();
  } catch (e) {
    port = new Worker(workerUrl);  // no SharedWorker: one server per tab
  }

  let nextId = 1;
  const waiting = new Map();
  const listeners = [];
  port.onmessage = (e) => {
    const m = e.data;
    if (m.id && waiting.has(m.id)) {
      const w = waiting.get(m.id);
      waiting.delete(m.id);
      if (m.ok) w.resolve(m);
      else w.reject(new Error(m.error));
    } else {
      for (const fn of listeners) fn(m);
    }
  };
  function ask(msg) {
    return new Promise((resolve, reject) => {
      msg.id = nextId++;
      waiting.set(msg.id, { resolve, reject });
      port.postMessage(msg);
    });
  }

  // ---- the session cookie ------------------------------------------------------------
  // The staff terminal and the public pages keep separate cookies, so a student tab
  // and a staff tab can be open side by side in one browser.
  const JARS = ["cafe-demo-cookies", "cafe-demo-cookies-staff"];
  const JAR = /\/staff\//.test(location.pathname) ? JARS[1] : JARS[0];
  function readJar() {
    try {
      return JSON.parse(localStorage.getItem(JAR) || "{}");
    } catch (e) {
      return {};
    }
  }
  function writeJar(jar) {
    try {
      localStorage.setItem(JAR, JSON.stringify(jar));
    } catch (e) { /* private mode: the session lasts until the page changes */ }
  }
  function cookieHeader() {
    const jar = readJar(), t = Date.now();
    return Object.keys(jar).filter((k) => jar[k].until > t).map((k) => k + "=" + jar[k].value).join("; ");
  }
  function takeCookie(setCookie) {
    const parts = setCookie.split(";").map((s) => s.trim());
    const eq = parts[0].indexOf("=");
    const name = parts[0].slice(0, eq), value = parts[0].slice(eq + 1);
    const age = parts.find((p) => /^max-age=/i.test(p));
    const seconds = age ? parseInt(age.split("=")[1], 10) : 86400;
    const jar = readJar();
    if (!value || seconds <= 0) delete jar[name];
    else jar[name] = { value, until: Date.now() + seconds * 1000 };
    writeJar(jar);
  }

  // ---- /api/ requests go to the C++ server in the worker -----------------------------
  const realFetch = window.fetch.bind(window);
  window.fetch = async (input, options) => {
    const url = typeof input === "string" ? input : input.url;
    if (!url.startsWith("/api/")) return realFetch(input, options);
    options = options || {};
    const q = url.indexOf("?");
    const headers = Object.assign({}, options.headers || {});
    const cookies = cookieHeader();
    if (cookies) headers.Cookie = cookies;
    let body = "", parts = [];
    if (options.body instanceof FormData) {
      const boundary = "demo";
      headers["Content-Type"] = "multipart/form-data; boundary=" + boundary;
      for (const [name, v] of options.body.entries()) {
        if (v instanceof Blob) parts.push({ name, filename: v.name || "file", type: v.type, data: new Uint8Array(await v.arrayBuffer()) });
        else parts.push({ name, data: new TextEncoder().encode(v) });
      }
    } else if (options.body !== undefined && options.body !== null) {
      body = String(options.body);
    }
    const m = await ask({
      type: "request",
      method: (options.method || "GET").toUpperCase(),
      path: q < 0 ? url : url.slice(0, q),
      query: q < 0 ? "" : url.slice(q + 1),
      headers: Object.keys(headers).map((k) => k + ": " + headers[k]).join("\n"),
      body,
      parts,
    });
    const res = m.res;
    const out = new Headers();
    for (const [k, v] of res.headers) {
      if (k.toLowerCase() === "set-cookie") takeCookie(v);
      else out.append(k, v);
    }
    showClock(m.now);
    if (/\/api\/(register|verify|reservations|waitlist|staff\/ready|staff\/collected|staff\/undo)/.test(url)) refreshInbox();
    const empty = res.status === 204 || res.status === 304;
    return new Response(empty ? null : res.body, { status: res.status, headers: out });
  };

  // ---- the demo bar ------------------------------------------------------------------
  const css = `
    .demo-bar{position:fixed;left:0;right:0;bottom:0;z-index:1000;background:#2A1410;color:#FFFAF3;
      font:14px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;padding:8px 16px;
      display:flex;flex-wrap:wrap;gap:8px 16px;align-items:center;box-shadow:0 -2px 8px #2A141040}
    .demo-bar p{margin:0;flex:1 1 320px}
    .demo-bar b{color:#F2B531}
    .demo-bar .demo-actions{display:flex;flex-wrap:wrap;gap:8px}
    .demo-bar button,.demo-accounts button{font:inherit;cursor:pointer;border-radius:6px;padding:6px 12px;
      border:1px solid #F2B531;background:#FFFAF3;color:#2A1410}
    .demo-bar button:focus-visible,.demo-accounts button:focus-visible{outline:3px solid #F2B531;outline-offset:2px}
    .demo-pad{height:96px}
    @media (max-width:640px){.demo-pad{height:176px}}
    .demo-accounts{margin:16px 0;padding:16px;border:2px dashed #B42318;border-radius:12px;background:#FFFAF3;color:#2A1410}
    .demo-accounts h2{font-size:18px;margin:0 0 8px}
    .demo-accounts p{margin:0 0 8px}
    .demo-accounts ul{list-style:none;margin:0;padding:0;display:grid;gap:8px}
    .demo-accounts li{display:flex;flex-wrap:wrap;gap:4px 12px;align-items:center}
    .demo-accounts code{background:#F2B531;color:#2A1410;padding:1px 6px;border-radius:4px}
    dialog.demo-inbox{max-width:min(720px,calc(100vw - 32px));width:100%;border:0;border-radius:12px;padding:0;color:#2A1410}
    dialog.demo-inbox::backdrop{background:#2A141099}
    .demo-inbox header{display:flex;justify-content:space-between;align-items:center;padding:12px 16px;background:#B42318;color:#FFFAF3}
    .demo-inbox h2{margin:0;font-size:18px}
    .demo-inbox header button{font:inherit;border:1px solid #FFFAF3;background:transparent;color:#FFFAF3;border-radius:6px;padding:4px 10px;cursor:pointer}
    .demo-inbox .demo-mails{padding:16px;max-height:70vh;overflow:auto;display:grid;gap:12px}
    .demo-inbox pre{white-space:pre-wrap;word-break:break-word;margin:0;padding:12px;background:#F6E6D2;border-radius:8px;font:13px/1.5 ui-monospace,Consolas,monospace}
  `;
  const style = document.createElement("style");
  style.textContent = css;
  document.head.append(style);

  const staffLogin = /\/staff\/login\.html$/.test(location.pathname);
  const userLogin = /\/login\.html$/.test(location.pathname) && !staffLogin;

  function button(text, onclick) {
    const b = document.createElement("button");
    b.type = "button";
    b.textContent = text;
    b.addEventListener("click", onclick);
    return b;
  }

  let clockNode, inboxButton, dialog, mails;
  function showClock(text) {
    if (clockNode && text) clockNode.textContent = text;
  }
  async function refreshInbox() {
    if (!inboxButton) return;
    try {
      const m = await ask({ type: "inbox" });
      inboxButton.textContent = "Email inbox (" + m.inbox.length + ")";
      if (dialog && dialog.open) fillInbox(m.inbox);
    } catch (e) { /* the bar keeps its old count */ }
  }
  function fillInbox(list) {
    mails.replaceChildren();
    if (!list.length) {
      const p = document.createElement("p");
      p.textContent = "No emails yet. Register, reserve or mark food ready and the emails the server sends appear here.";
      mails.append(p);
    }
    for (const mail of list) {
      const pre = document.createElement("pre");
      pre.textContent = mail.text;
      mails.append(pre);
    }
  }

  function buildBar() {
    const bar = document.createElement("aside");
    bar.className = "demo-bar";
    bar.setAttribute("aria-label", "Live demo");
    const p = document.createElement("p");
    p.append("Live demo: the real C++ server, compiled to WebAssembly, runs in your browser. Cafeteria clock: ");
    clockNode = document.createElement("b");
    clockNode.textContent = "starting…";
    p.append(clockNode);
    const actions = document.createElement("div");
    actions.className = "demo-actions";
    inboxButton = button("Email inbox", async () => {
      const m = await ask({ type: "inbox" });
      fillInbox(m.inbox);
      dialog.showModal();
    });
    const hour = button("+1 hour", async (e) => {
      e.currentTarget.disabled = true;
      await window.fetch("/api/dev/advance", { method: "POST", headers: { "X-Cafe": "1", "Content-Type": "application/x-www-form-urlencoded" }, body: "minutes=60" });
      location.reload();
    });
    const reset = button("Reset demo", async () => {
      if (!confirm("Start the demo cafeteria again from 08:30? Every order, account and email you made in this browser is removed.")) return;
      try { JARS.forEach((k) => localStorage.removeItem(k)); } catch (e) { /* nothing stored */ }
      await ask({ type: "reset" }).catch(() => {});
    });
    actions.append(inboxButton, hour, reset);
    bar.append(p, actions);

    dialog = document.createElement("dialog");
    dialog.className = "demo-inbox";
    dialog.setAttribute("aria-labelledby", "demo-inbox-title");
    const head = document.createElement("header");
    const h = document.createElement("h2");
    h.id = "demo-inbox-title";
    h.textContent = "Emails the server sent (newest first)";
    head.append(h, button("Close", () => dialog.close()));
    mails = document.createElement("div");
    mails.className = "demo-mails";
    dialog.append(head, mails);

    const pad = document.createElement("div");
    pad.className = "demo-pad";
    document.body.append(pad, bar, dialog);
  }

  function buildAccounts() {
    const form = document.getElementById("login-form");
    if (!form || !(staffLogin || userLogin)) return;
    const box = document.createElement("section");
    box.className = "demo-accounts";
    box.setAttribute("aria-labelledby", "demo-accounts-title");
    const h = document.createElement("h2");
    h.id = "demo-accounts-title";
    h.textContent = staffLogin ? "Demo account: cafeteria staff" : "Demo accounts: students, teachers and university staff";
    const intro = document.createElement("p");
    intro.textContent = staffLogin
      ? "This is the cafeteria staff terminal. Students, teachers and university staff log in on the public login page."
      : "Pick one to fill in the form, then press Log in. Cafeteria staff use the staff login page instead.";
    const ul = document.createElement("ul");
    const accounts = staffLogin
      ? [["Cafeteria staff", "demo.staff", "demo-staff-pass"]]
      : [["Student", "ayon@uni.example", "demo-student-pass"],
         ["Teacher", "teacher1@uni.example", "demo-teacher-pass"],
         ["University staff", "guard1@uni.example", "demo-guard-pass"],
         ["University staff, no university email", "rahim@example.com", "demo-driver-pass"]];
    for (const [who, login, pass] of accounts) {
      const li = document.createElement("li");
      const name = document.createElement("strong");
      name.textContent = who + ":";
      const l = document.createElement("code");
      l.textContent = login;
      const pw = document.createElement("code");
      pw.textContent = pass;
      li.append(name, l, pw, button("Fill in", () => {
        form.elements[staffLogin ? "login" : "email"].value = login;
        form.elements.password.value = pass;
        form.querySelector("button[type=submit]").focus();
      }));
      ul.append(li);
    }
    box.append(h, intro, ul);
    if (userLogin) {
      const more = document.createElement("p");
      more.textContent = "To try registering, create an account for ali@uni.example with ID 1072520005101050; the 6-digit code arrives in the Email inbox below.";
      box.append(more);
    }
    form.after(box);
  }

  listeners.push((m) => {
    if (m.type === "clock") showClock(m.now);
    if (m.type === "reset") setTimeout(() => location.reload(), 300);
  });

  document.addEventListener("DOMContentLoaded", () => {
    buildBar();
    buildAccounts();
    ask({ type: "now" }).then((m) => showClock(m.now)).catch((e) => showClock("could not start (" + e.message + ")"));
    refreshInbox();
  });
})();
