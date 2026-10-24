"use strict";

document.getElementById("login-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const form = e.target;
  const r = await api.post("/api/staff/login", {
    login: form.login.value.trim(),
    password: form.password.value,
  });
  if (r.ok) {
    window.location.href = "/staff/portal.html";
  } else {
    say("msg", r.error, false);
    form.password.value = "";
    form.password.focus();
  }
});
