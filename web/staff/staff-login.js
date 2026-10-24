"use strict";

document.getElementById("login-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const form = e.target;
  say("msg", "");
  if (!checkFields(form)) return;
  const done = busy(form.querySelector("button[type=submit]"), "Logging in…");
  const r = await api.post("/api/staff/login", {
    login: form.login.value.trim(),
    password: form.password.value,
  });
  if (r.ok) {
    window.location.href = "/staff/portal.html";
    return;
  }
  done();
  say("msg", r.error, false);
  form.password.value = "";
  form.password.focus();
});
