"use strict";

document.getElementById("login-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const form = e.target;
  const button = form.querySelector("button");
  button.disabled = true;
  say("msg", "Logging in…", true);
  const r = await api.post("/api/login", {
    email: form.email.value.trim(),
    password: form.password.value,
  });
  button.disabled = false;
  if (r.ok) {
    window.location.href = "/menu.html";
  } else {
    say("msg", r.error);
    form.password.value = "";
    form.password.focus();
  }
});
