"use strict";

async function loadDetails() {
  const me = await api.get("/api/me");
  if (!requireLogin(me, "/login.html")) return;
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
  const current = document.getElementById("current").value;
  const next = document.getElementById("new1").value;
  if (next !== document.getElementById("new2").value) {
    say("msg", "The two new passwords are different.");
    document.getElementById("new2").focus();
    return;
  }
  const r = await api.post("/api/account/password", { current, password: next });
  // 401 here is also the answer to a wrong current password, so ask who is signed in first.
  if (r.status === 403 || (r.status === 401 && !requireLogin(await api.get("/api/me"), "/login.html"))) return;
  say("msg", r.ok ? r.message : r.error, r.ok);
  if (r.ok) form.reset();
});

wireLogout("logout", "/");
loadDetails();
fillAnnouncement();
