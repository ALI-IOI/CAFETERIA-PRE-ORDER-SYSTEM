"use strict";

const step1 = document.getElementById("step1");
const step2 = document.getElementById("step2");
let email = "";
let password = "";  // kept in memory for step 2: the server checks it again with the code

step1.addEventListener("submit", async (e) => {
  e.preventDefault();
  email = step1.email.value.trim();
  const button = step1.querySelector("button");
  button.disabled = true;
  say("msg1", "Checking with the university list…", true);
  const r = await api.post("/api/register", {
    email,
    id: document.getElementById("uid").value.trim(),  // step1.id would be the form's own id
    password: step1.password.value,
  });
  password = step1.password.value;
  button.disabled = false;
  if (!r.ok) {
    say("msg1", r.error, false);
    return;
  }
  document.getElementById("sent-to").textContent = email;
  step1.hidden = true;
  step2.hidden = false;
  step2.code.focus();
});

step2.addEventListener("submit", async (e) => {
  e.preventDefault();
  const r = await api.post("/api/verify", { email, code: step2.code.value.trim(), password });
  if (!r.ok) {
    say("msg2", r.error, false);
    step2.code.select();
    return;
  }
  say("msg2", r.message + " Taking you to the login page…", true);
  setTimeout(() => (window.location.href = "/login.html"), 1500);
});

document.getElementById("restart").addEventListener("click", () => {
  step2.hidden = true;
  step1.hidden = false;
  step1.email.focus();
});
