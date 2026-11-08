"use strict";

// The token board: the screen at the counter. Public, no login, token numbers
// only. Ready tokens come in the order they became ready (the server sorts
// them), so the first is the one that has waited longest.

let shownReady = null;  // tokens on the board at the last draw, so a newly called one is announced

async function draw() {
  const state = document.getElementById("board-state");
  const board = document.getElementById("board");
  if (shownReady === null) listState(state, "loading");
  let b;
  try {
    b = await api.get("/api/tokens/board");
  } catch (e) {
    b = { ok: false };
  }
  if (!b.ok || !Array.isArray(b.ready) || !Array.isArray(b.cooking)) {
    listState(state, "error", "Couldn’t load the token board. It will try again in a few seconds.", draw);
    board.hidden = shownReady === null;
    return;
  }
  listState(state, null);
  board.hidden = false;

  const before = shownReady || new Set();
  const called = [];
  document.getElementById("ready").replaceChildren(...b.ready.map((t, i) => {
    const isNew = shownReady !== null && !before.has(t.token);
    if (isNew) called.push(t.text);
    const stamp = tokenStamp(i === 0 ? "Now serving" : "Ready", t.text, i === 0 ? "Next at the counter" : "", "ready", i === 0 ? "big" : "");
    return el("li", { class: isNew ? "is-new" : null }, stamp);
  }));
  document.getElementById("ready-empty").hidden = b.ready.length > 0;
  shownReady = new Set(b.ready.map((t) => t.token));

  document.getElementById("cooking").replaceChildren(...b.cooking.map((t) =>
    el("li", {},
      el("span", { class: "figure" }, t.text),
      el("small", {}, t.dishes > 1 ? t.readyDishes + " of " + t.dishes + " ready" : "Cooking"))));
  document.getElementById("cooking-empty").hidden = b.cooking.length > 0;

  if (called.length) {
    document.getElementById("called").textContent =
      (called.length === 1 ? "Token " + called[0] + " is" : "Tokens " + called.join(", ") + " are") + " ready.";
  }
  document.getElementById("updated").textContent = "Updated " + b.now + " (server time)";
}

draw();
setInterval(() => { if (!document.hidden) draw(); }, 5000);
