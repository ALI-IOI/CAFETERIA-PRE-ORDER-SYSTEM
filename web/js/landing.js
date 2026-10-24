"use strict";

// Fills the hero and the "Today’s info" strip from the public endpoint
// (no login, no personal data). The page reads fine if it fails.
(async () => {
  const info = await api.get("/api/public/info");
  if (!info.ok) return;
  document.getElementById("hero-cutoff").textContent = info.cutoff;
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
