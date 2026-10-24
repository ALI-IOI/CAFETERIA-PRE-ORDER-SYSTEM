"use strict";

async function loadDetails() {
  let me;
  try {
    me = await api.get("/api/me");
  } catch (e) {
    me = { ok: false, status: 0 };
  }
  if (!requireLogin(me, "/login.html")) return;
  if (!me.ok) {
    say("msg", "Couldn't load your details. Reload the page to try again.", false);
    return;
  }
  const rows = [
    ["Name", me.name],
    ["Role", roleLabel(me.role)],
    ["Email", me.login],
  ];
  if (me.role === "student") rows.push(["Student ID", me.universityId]);
  if (me.department) rows.push(["Department", me.department]);
  document.getElementById("details").replaceChildren(...rows.flatMap(([k, v]) => [el("dt", {}, k), el("dd", {}, v)]));
}

document.getElementById("password-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const form = e.target;
  say("msg", "");
  if (!checkFields(form)) return;
  const new2 = document.getElementById("new2");
  const next = document.getElementById("new1").value;
  if (next !== new2.value) {
    fieldError(new2, "The two new passwords are different. Type the same one twice.");
    new2.focus();
    return;
  }
  const done = busy(form.querySelector("button[type=submit]"), "Changing…");
  const r = await api.post("/api/account/password", { current: document.getElementById("current").value, password: next });
  done();
  // 401 here is also the answer to a wrong current password, so ask who is signed in first.
  if (r.status === 403 || (r.status === 401 && !requireLogin(await api.get("/api/me"), "/login.html"))) return;
  say("msg", r.ok ? r.message : r.error, r.ok);
  if (r.ok) form.reset();
});

wireLogout("logout", "/");
loadDetails();
fillAnnouncement();
