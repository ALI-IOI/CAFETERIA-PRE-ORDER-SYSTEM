// Live demo: runs the C++ cafeteria server (cafe_demo.wasm, built from src/ by
// web-demo/wasm.cmake) in a worker shared by every open page of the site.
// Its data files are kept in this browser's IndexedDB, so each visitor has a
// private cafeteria that survives page loads and visits.
"use strict";

importScripts("cafe_demo.js");

const BASE = self.location.pathname.replace(/\/demo\/cafe-worker\.js$/, "");
const ports = [];
let mod = null;
let queue = Promise.resolve();  // one request at a time, like the server's mutex

function sync(load) {
  return new Promise((resolve, reject) => mod.FS.syncfs(load, (err) => (err ? reject(err) : resolve())));
}

const now = () => Date.now() / 1000;

function withBytes(bytes, fn) {
  const ptr = mod._malloc(bytes.length || 1);
  mod.HEAPU8.set(bytes, ptr);
  try {
    return fn(ptr, bytes.length);
  } finally {
    mod._free(ptr);
  }
}

const ready = createCafe({ locateFile: (f) => f, print: () => {}, printErr: (t) => console.warn("[cafe]", t) }).then(async (m) => {
  mod = m;
  m.FS.mkdir("/data");
  m.FS.mount(m.IDBFS, {}, "/data");
  await sync(true);
  const d = new Date();
  const rc = m.ccall("cafe_boot", "number", ["number", "number", "number", "number"],
    [d.getFullYear(), d.getMonth() + 1, d.getDate(), now()]);
  if (rc !== 0) throw new Error("The demo cafeteria could not start (code " + rc + ").");
  await sync(false);
  setInterval(() => {  // the server's minute timer
    queue = queue.then(async () => {
      m.ccall("cafe_tick", null, ["number"], [now()]);
      await sync(false);
      broadcast({ type: "clock", now: m.ccall("cafe_now", "string", [], []) });
    });
  }, 60000);
});

function broadcast(msg) {
  for (const p of ports) p.postMessage(msg);
}

// Pictures the C++ code stored in /data/images become data: URLs; the drawings
// in web/img are files of the site.
function fixUrls(value) {
  if (typeof value === "string") {
    if (value.startsWith("/images/")) {
      const name = value.slice(8);
      try {
        const bytes = mod.FS.readFile("/data/images/" + name);
        const ext = name.split(".").pop().toLowerCase();
        const type = ext === "png" ? "image/png" : ext === "webp" ? "image/webp" : "image/jpeg";
        let bin = "";
        for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
        return "data:" + type + ";base64," + btoa(bin);
      } catch (e) {
        return BASE + "/img/demo/bowl.svg";
      }
    }
    if (value.startsWith("/img/")) return BASE + value;
    return value;
  }
  if (Array.isArray(value)) return value.map(fixUrls);
  if (value && typeof value === "object") {
    for (const k of Object.keys(value)) value[k] = fixUrls(value[k]);
  }
  return value;
}

function request(msg) {
  for (const part of msg.parts || []) {
    withBytes(part.data, (ptr, len) =>
      mod.ccall("cafe_add_part", null, ["string", "string", "string", "number", "number"],
        [part.name, part.filename || "", part.type || "", ptr, len]));
  }
  const body = new TextEncoder().encode(msg.body || "");
  const out = withBytes(body, (ptr, len) =>
    mod.ccall("cafe_request", "string", ["string", "string", "string", "string", "number", "number", "number"],
      [msg.method, msg.path, msg.query || "", msg.headers || "", ptr, len, now()]));
  const res = JSON.parse(out);
  const type = (res.headers.find((h) => h[0].toLowerCase() === "content-type") || [, ""])[1];
  if (type.startsWith("application/json") && res.body) {
    try {
      res.body = JSON.stringify(fixUrls(JSON.parse(res.body)));
    } catch (e) { /* leave it as the server wrote it */ }
  }
  return res;
}

// Emails the server wrote to data/outbox, newest first.
function inbox() {
  let names = [];
  try {
    names = mod.FS.readdir("/data/outbox").filter((n) => n.endsWith(".txt"));
  } catch (e) {
    return [];
  }
  names.sort().reverse();
  return names.slice(0, 30).map((n) => ({ name: n, text: mod.FS.readFile("/data/outbox/" + n, { encoding: "utf8" }) }));
}

function wipe(dir) {
  for (const n of mod.FS.readdir(dir)) {
    if (n === "." || n === "..") continue;
    const p = dir + "/" + n;
    if (mod.FS.isDir(mod.FS.stat(p).mode)) {
      wipe(p);
      mod.FS.rmdir(p);
    } else mod.FS.unlink(p);
  }
}

function handle(port, msg) {
  queue = queue.then(async () => {
    try {
      await ready;
      if (msg.type === "request") {
        const res = request(msg);
        if (msg.method !== "GET") await sync(false);
        port.postMessage({ id: msg.id, ok: true, res, now: mod.ccall("cafe_now", "string", [], []) });
      } else if (msg.type === "inbox") {
        port.postMessage({ id: msg.id, ok: true, inbox: inbox() });
      } else if (msg.type === "now") {
        port.postMessage({ id: msg.id, ok: true, now: mod.ccall("cafe_now", "string", [], []) });
      } else if (msg.type === "reset") {
        wipe("/data");
        await sync(false);
        broadcast({ type: "reset" });
        setTimeout(() => self.close(), 50);
      }
    } catch (e) {
      port.postMessage({ id: msg.id, ok: false, error: String(e && e.message ? e.message : e) });
    }
  });
}

function connect(port) {
  ports.push(port);
  port.onmessage = (e) => handle(port, e.data);
  if (port.start) port.start();
}

if (typeof SharedWorkerGlobalScope !== "undefined" && self instanceof SharedWorkerGlobalScope) {
  self.onconnect = (e) => connect(e.ports[0]);
} else {
  connect(self);  // dedicated worker (browsers without SharedWorker)
}
