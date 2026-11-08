"use strict";

// Fills the home field and the "Today’s times" stamps from the public endpoints
// (no login, no personal data). The page reads fine if either fails.
(async () => {
  const info = await api.get("/api/public/info");
  if (!info.ok) return;
  const status = document.getElementById("hero-status");
  status.textContent = info.open
    ? "Orders are open until " + info.cutoff + "."
    : "Orders are closed for today. You can order again tomorrow morning.";
  status.hidden = false;
  for (const node of document.querySelectorAll(".js-cutoff")) node.textContent = info.cutoff;
  for (const node of document.querySelectorAll(".js-freshness")) node.textContent = info.freshness;
  document.getElementById("info-cutoff").textContent = info.cutoff;
  document.getElementById("info-cancel").textContent = info.cancelDeadline;
  document.getElementById("info-fresh").textContent = info.freshness;
  document.getElementById("info-now").textContent = info.now;
})();

// "Now serving": the token that has waited longest at the counter, refreshed
// every 15 seconds while the page is open.
async function fillServing() {
  const box = document.getElementById("serving");
  const b = await api.get("/api/tokens/board");
  if (!b.ok) return;
  let stamp;
  if (b.ready.length) {
    const more = b.ready.length - 1;
    stamp = tokenStamp("Now serving", b.ready[0].text,
      more ? more + (more === 1 ? " more token ready" : " more tokens ready") : "Show it at the counter", "cream", "big");
  } else {
    stamp = tokenStamp("Now serving", "–",
      b.cooking.length ? b.cooking.length + (b.cooking.length === 1 ? " token cooking" : " tokens cooking") : "No food waiting",
      "cream", "big");
  }
  box.firstElementChild.replaceWith(stamp);
}
fillServing();
setInterval(() => { if (!document.hidden) fillServing(); }, 15000);
