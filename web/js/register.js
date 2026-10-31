"use strict";

const step1 = document.getElementById("step1");
const step2 = document.getElementById("step2");
let email = "";
let password = "";  // kept in memory for step 2: the server checks it again with the code

// The two-part line above the heading follows the form that is showing.
function showStep(n) {
  const [first, second] = document.querySelectorAll("#progress li");
  document.getElementById("progress").setAttribute("aria-label", "Step " + n + " of 2");
  first.classList.toggle("is-done", n === 2);
  if (n === 1) { first.setAttribute("aria-current", "step"); second.removeAttribute("aria-current"); }
  else { second.setAttribute("aria-current", "step"); first.removeAttribute("aria-current"); }
  step1.hidden = n !== 1;
  step2.hidden = n !== 2;
}

step1.addEventListener("submit", async (e) => {
  e.preventDefault();
  say("msg1", "");
  if (!checkFields(step1)) return;
  email = step1.email.value.trim();
  const done = busy(step1.querySelector("button[type=submit]"), "Checking with the university list…");
  const r = await api.post("/api/register", {
    email,
    id: document.getElementById("uid").value.trim(),  // step1.id would be the form's own id
    password: step1.password.value,
  });
  password = step1.password.value;
  done();
  if (!r.ok) {
    say("msg1", r.error, false);
    return;
  }
  document.getElementById("sent-to").textContent = email;
  showStep(2);
  step2.code.focus();
});

step2.addEventListener("submit", async (e) => {
  e.preventDefault();
  say("msg2", "");
  if (!checkFields(step2)) return;
  const done = busy(step2.querySelector("button[type=submit]"), "Verifying…");
  const r = await api.post("/api/verify", { email, code: step2.code.value.trim(), password });
  if (!r.ok) {
    done();
    say("msg2", r.error, false);
    step2.code.select();
    return;
  }
  say("msg2", r.message, true);
  // a link the person follows when ready, not a timed jump (WCAG 2.2.1)
  const go = el("a", { class: "btn btn--primary btn--block", href: "/login.html" }, "Go to the login page");
  step2.querySelector("button[type=submit]").replaceWith(go);
  go.focus();
});

document.getElementById("restart").addEventListener("click", () => {
  showStep(1);
  step1.email.focus();
});
