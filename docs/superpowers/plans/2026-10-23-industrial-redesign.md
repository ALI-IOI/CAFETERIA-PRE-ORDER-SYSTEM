# Industrial Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Staff-managed menu items with price, picture and availability; a filterable menu page with an all-or-nothing tray; a public landing page; and the green-and-stainless industrial look on every page.

**Architecture:** Core (`src/core`) gains item management and a batch reservation on the existing hand-built array/list/queues; storage gains three menu fields and an image store; the server adds a public info route, a batch route, staff item routes and an `/images` mount; the web layer is rebuilt on one new CSS design system with vanilla JS.

**Tech Stack:** C++17 (MinGW g++ 13.1, CMake/Ninja), cpp-httplib v0.60.1 (`req.form.get_file/get_field` for multipart), libsodium (random file names), HTML/CSS/vanilla JS, self-hosted Oswald 500 (OFL).

**Spec:** `docs/superpowers/specs/2026-10-23-industrial-redesign-design.md`

## Global Constraints
- No STL containers in `src/ds|core|auth|storage|console|email` (guard test). No framework in `web/`.
- No payment, delivery, ratings, favourites, deleting items. Prices are display only: "pay at the counter".
- Palette exactly: Graphite `#16191C`, Stainless `#C8CDD2`, Steel `#6B737B`, Light steel `#E2E5E8`, Concrete `#F1F2F3`, Machine green `#1F6B45`, Deep green `#0F5C3A` (green text on stainless only in deep green).
- Headings: Oswald 500 uppercase from `web/fonts/`; CSP stays `default-src 'self'`.
- Placeholders: array capacity `MAX_MENU_ITEMS = 32` `[CONFIRM]`; picture limit `MAX_IMAGE_BYTES = 2 * 1024 * 1024` `[CONFIRM]`.
- All server text into pages via `textContent`/`el()`; never `innerHTML`.
- Every existing test keeps passing (unit, console demo, admin tool, E2E).

## Review Focus
1. A renamed non-image (HTML, EXE) or a 10 MB file uploaded as a picture → refused, nothing written, and every other route still refuses bodies over 4 KB (Task 3 tests `fake_image_refused`, Task 4 E2E `non_image_upload_refused`, `other_routes_keep_4kb_limit`).
2. A tray with the same dish on two lines, or lines that together pass the per-person limit → refused as a whole (Task 2 `batch_counts_earlier_lines_of_same_dish`).
3. A dish switched off while it sits in someone's tray → the batch is refused naming that dish, nothing reserved (Task 2 `batch_refuses_unavailable_dish`; Task 6 page drops it from the tray on reload).
4. Staff typing `<script>`, quotes or `|` in an item name → `|`/control chars refused by validation; anything else shown as text (Task 1 `item_name_validation`; Task 4 E2E `item_name_shown_as_text`).
5. Restart with staff-added items and pictures → items, prices, availability and pictures restored; old 5-field `menu.txt` still loads (Task 1 `menu_round_trip_new_fields`, `old_five_field_menu_loads`).

---

### Task 1: Item fields, availability, staff item actions

**Files:** Modify `src/ds/menu_array.h`, `src/ds/action_stack.h`, `src/core/cafeteria.h/.cpp`, `src/core/staff.cpp`, `src/core/waitlist.cpp`, `src/storage/storage.cpp`, `src/auth/validation.h/.cpp` (reuse `validName`). Tests: `tests/test_menu_items.cpp`, additions in `tests/test_storage.cpp`.

**Interfaces — Produces:**
- `MenuItem` += `int priceTaka; std::string imageFile; bool available;` (defaults 0, "", true).
- `ActionType` += `SetPrice, SetAvailable`.
- `Result` += `NotAvailable, MenuFull`.
- `Result addMenuItem(Cafeteria&, const std::string& staff, const std::string& category, const std::string& name, ItemKind kind, int priceTaka, int stockOrLimit, int& newIndex);` — validates `validName(category)`, `validName(name)`, unique name, price 0–9999, stock 0–9999 or limit 0..cookingCapacity; `MenuFull` at capacity; not on the undo stack.
- `Result setPrice(Cafeteria&, staff, int item, int priceTaka, bool& undoRecorded);` and `Result setAvailable(Cafeteria&, staff, int item, bool available, NotificationList& out, bool& undoRecorded);` — both undoable; turning available on calls `promoteWaitlist`.
- `Result setItemImage(Cafeteria&, staff, int item, const std::string& fileName);` — not undoable.
- `reserveCore`/`reserveWithNote` and `joinWaitlist` return `NotAvailable` (checked right after `NotFresh`); `promoteWaitlist` returns 0 for an unavailable item (waitlist pauses).
- `menu.txt`: `category|name|kind|stock|dailyLimit|priceTaka|imageFile|available` (`available` = 1/0); 5-field lines load with 0, "", 1.

- [ ] Tests first: `add_item_appends_at_next_index`, `add_item_refused_when_full` (fill to 32 → `MenuFull`), `add_item_duplicate_name_refused`, `item_name_validation` (`"Ta|hari"`, `"\n"`, 61 chars refused; `"<b>Tehari</b>"` accepted), `set_price_and_undo`, `set_available_off_refuses_reserve_and_waitlist` (both `NotAvailable`), `existing_reservation_survives_unavailable` (still markReady-able), `waitlist_pauses_while_unavailable` (cancel frees portion → nobody promoted; set available → promoted), `undo_set_available`, `menu_round_trip_new_fields`, `old_five_field_menu_loads`. Run → compile failure. Implement. Run `build/unit_tests.exe` → all pass. Commit `feat(core): staff-managed menu items with price, picture name and availability`.

### Task 2: All-or-nothing batch reservation

**Files:** Modify `src/core/cafeteria.h/.cpp`. Test: `tests/test_batch.cpp`.

**Interfaces — Consumes:** Task 1 `NotAvailable`. **Produces:**
- `struct TrayLine { int itemIndex; int quantity; };`
- `Result reserveBatch(Cafeteria&, const std::string& user, const TrayLine* lines, int n, NotificationList& out, int& failedLine, int* createdIds);` — `n` 1..`MAX_MENU_ITEMS`; checks every line in order with the single-dish rules, counting portions from earlier lines of the same tray (remaining and per-person); on the first failure returns its `Result`, sets `failedLine`, and changes nothing; otherwise makes every reservation with no per-dish email and appends **one** `Confirmation` email: subject `"Reservations confirmed: <codes joined by ", ">"`, body one line per dish `"R-0012  Tehari x 2  ৳ 240"` then `"Total to pay at the counter: ৳ <sum>"`.
- Internal `Result reserveCore(Cafeteria&, user, item, qty, Reservation** created)` shared by `reserveWithNote` (which adds the email) and `reserveBatch`.

- [ ] Tests first: `batch_all_ok_one_email_with_total`, `batch_counts_earlier_lines_of_same_dish` (Tehari 1 + Tehari 2 → `UserLimit`, `failedLine == 1`, nothing reserved), `batch_sold_out_line_reserves_nothing` (reservedToday unchanged for every item, list size unchanged, no email), `batch_refuses_unavailable_dish`, `batch_after_cutoff_refused`, `batch_queue_full_reserves_nothing`, `batch_removes_user_from_each_dishs_waitlist`. Run → fail. Implement (validate pass over a small fixed array of per-item running totals, then apply pass). Run → pass. Commit.

### Task 3: Picture checks and image store

**Files:** Create `src/storage/image_store.h/.cpp`. Test: `tests/test_image_store.cpp`.

**Interfaces — Produces:**
- `enum class ImageKind { None, Jpeg, Png, Webp };` `ImageKind detectImage(const std::string& bytes);` — JPEG `FF D8 FF`, PNG `89 50 4E 47 0D 0A 1A 0A`, WebP `RIFF????WEBP`.
- `const size_t MAX_IMAGE_BYTES = 2 * 1024 * 1024;`
- `bool storeItemImage(const std::string& dataDir, int itemIndex, const std::string& bytes, const std::string& oldFile, std::string& newFile, std::string& err);` — refuses empty, oversized or `None`; writes `images/item-<index>-<16 hex>.<jpg|png|webp>` atomically (`randomToken(8)`), then deletes `images/<oldFile>` if `oldFile` matches `item-*`.
- `bool validImageFileName(const std::string& name);` (`item-<digits>-<hex>.<ext>` only; used when loading `menu.txt`).

- [ ] Tests first: `jpeg_png_webp_detected`, `fake_image_refused` (`"<html>"`, `"MZ\x90"`, a PNG header truncated to 4 bytes), `oversized_refused`, `stored_name_is_server_chosen` (matches `validImageFileName`, file exists), `old_picture_deleted_on_replace`, `path_like_old_name_not_deleted` (`"../menu.txt"` left alone). Run → fail; implement; pass; commit.

### Task 4: Server routes

**Files:** Modify `src/server/routes.cpp`, `src/server/main.cpp`. Test: `tests/e2e/e2e_section12.cpp` (new block at the end, before shutdown).

**Interfaces — Consumes:** Tasks 1–3. **Produces (JSON fields the pages use):**
- `GET /api/public/info` (no login): `{ok, now, cutoff, cancelDeadline, freshness}`.
- `GET /api/menu`: hides unavailable items; each item adds `price`, `image` (`"/images/<file>"` or `"/img/demo/<bowl|chips|can|bottle>.svg"` by category/kind: Fresh food→bowl, Chips→chips, Soft drinks→can, otherwise bottle), `category`.
- `POST /api/reservations/batch` form `items=<i>:<q>,<i>:<q>` → `{ok, codes:[...], total}` or 4xx `{ok:false, error:"<dish name>: <reason>"}`.
- Staff: `POST /api/staff/items` (category, name, kind=fresh|packaged, price, amount); `POST /api/staff/items/price` (item, price); `POST /api/staff/items/available` (item, available=1|0); `POST /api/staff/items/image` (multipart: field `item`, file `image`). `GET /api/staff/overview` items add `price`, `image`, `available`.
- `svr.set_payload_max_length(MAX_IMAGE_BYTES + 64 KB)`; `handle()` refuses bodies over 4096 bytes with 413 except on `/api/staff/items/image`. Mount `/images` → `<data>/images` (created at start).
- Page guard: `/menu.html` and `/reservations.html` need a user session; `/home.html` redirects to `/menu.html`.

- [ ] E2E checks first: `public_info_without_login`, `staff_adds_item_student_sees_it` (price shown), `upload_picture_then_served` (PNG bytes → 200, `GET image` → 200 `image/png`), `non_image_upload_refused`, `student_cannot_upload` (403), `other_routes_keep_4kb_limit` (5 KB register body → 413), `item_name_shown_as_text` (name `<b>X</b>` returned JSON-escaped), `unavailable_item_hidden_and_refused`, `tray_batch_all_or_nothing` (one bad line → nothing reserved, error names dish; valid tray → codes and total, one confirmation email). Run E2E → those FAIL. Implement. Run `ctest` → 5/5. Commit.

### Task 5: Assets and design system

**Files:** Create `web/fonts/Oswald-500.woff2`, `web/fonts/OFL.txt` (downloaded once from Google Fonts / google/fonts repo), `web/img/demo/{bowl,chips,can,bottle}.svg`, `web/img/campus-hero.svg` (labelled stainless placeholder "BAUST Khulna photo — add web/img/campus-hero.jpg"); rewrite `web/css/app.css`.

- CSS tokens = Global Constraints palette; `@font-face` Oswald; components: `.topbar` (graphite, green rule), `.hero` (stainless; `background-image: url(/img/campus-hero.jpg), url(/img/campus-hero.svg)` so dropping in the JPG replaces the placeholder), `.btn` (graphite), `.btn--primary` (machine green, one per view), `.btn--ghost`, `.tile`, `.dish-card`, `.tabs`, `.tray`, `.chip--<Status>`, `.panel` (staff), dark-mode block, reduced-motion block. Keep skip link, focus ring, 44 px targets.
- [ ] Verify: contrast script (`tools/contrast_check.py`, the pairs from the spec) prints all PASS; `no_stl_guard` unaffected; commit.

### Task 6: Pages

**Files:** Rewrite `web/index.html` (landing) ; create `web/login.html`, `web/menu.html`, `web/js/landing.js`, `web/js/menu.js`, `web/js/tray.js`; delete `web/home.html`, `web/js/home.js`; restyle `web/register.html`, `web/reservations.html`, `web/staff/login.html`, `web/staff/portal.html` (+ "Menu items" panel in `web/js/staff-portal.js`); update `web/js/login.js` (→ `/menu.html`), `web/js/register.js` (→ `/login.html`).

- Landing: hero headline "Fresh food, reserved before {cutoff}" from `/api/public/info`; **Log in** (primary) and **Create account**; "How it works" 3 steps; "Today's info" strip.
- Menu: tabs from categories + "All"; search (name contains, case-insensitive); sort name / portions left / price; cards per spec; sold out → existing waitlist box; `tray.js` keeps `{itemIndex: qty}` in `sessionStorage`, drops lines whose item is now hidden, caps qty at the per-person limit minus portions held, shows line prices and total, **Reserve all** → batch route, shows the error naming the dish.
- Staff "Menu items" panel: add form (category, name, fresh/packaged, price, amount), per-item price field, available toggle, picture upload (`<input type="file" accept="image/jpeg,image/png,image/webp">`).
- [ ] Verify in the browser pane on the demo server: landing (no login), register → login → menu (tabs, search, sort, tray add/reserve-all, waitlist on sold-out), reservations, staff portal (add item with picture, toggle availability, price change + undo); repeat at 375 px; keyboard-only pass. Then `ctest` → 5/5. Commit.

### Task 7: Accessibility pass, docs, final review

- [ ] `design:accessibility-review` on the new pages; fix and re-run the contrast script.
- [ ] Update `README.md`, `demo/README.md` (landing at `/`, menu at `/menu.html`), `docs/testing/test-report.md` with fresh outputs.
- [ ] Fresh reviewer (most capable model) on the whole range with this plan's Review Focus; fix pass test-first; clean rebuild in a new folder; `ctest` 5/5; commit.
