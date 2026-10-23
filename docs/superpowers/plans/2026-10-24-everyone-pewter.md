# Everyone at the University, Hidden Staff Terminal, Eggshell Theme — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let every university employee order (self sign-up or admin-created), make the staff terminal invisible outside the cafeteria's own computers, and replace both colour themes with one eggshell theme (white top bar, light stainless lines, deep green).

**Architecture:** A new `Role::Employee` flows through the existing users and university linked lists; `addPerson` joins `addStaff` in `src/auth/auth.cpp` and the admin tool. A fixed-array `StaffComputers` list (new `src/auth/staff_computers.*`) is checked in the server's pre-routing handler, which answers like a missing page. The theme is a token rewrite of `web/css/app.css` plus an announcement bar filled from `/api/public/info`.

**Tech Stack:** C++17, MinGW-w64 g++ 13.1, CMake + Ninja, CTest; cpp-httplib (vendored), libsodium; plain HTML/CSS/JS.

**Spec:** `docs/superpowers/specs/2026-10-24-everyone-pewter-design.md`

## Global Constraints

- No STL containers in `src/ds`, `src/core`, `src/auth`, `src/storage`, `src/console`, `src/email` (the `no_stl_guard` test). `StaffComputers` is a fixed array.
- Role stored as `employee`, shown as "University staff". Cafeteria staff stay `Role::Staff`.
- Blank email allowed **only** on `employee` rows of `university_list.csv`.
- Self sign-up refusal copy, exact: `You have no university email. Ask the cafeteria office to create your account.`
- Start-up notice, exact: `NOTICE: config/staff_computers.txt is empty - the staff terminal is reachable from any computer.`
- Announcement copy, exact: `Orders open until HH:MM · pay at the counter` / `Orders closed for today · back tomorrow`.
- Colour tokens and contrast pairs exactly as the spec's Part 3 tables. No `prefers-color-scheme: dark` block; `:root { color-scheme: light; }`; `--radius: 0`.
- `textContent`-only rendering in JS; CSP unchanged.
- Every new test is seen failing before the code that passes it.
- [CONFIRM] values stay in brackets: counter computer addresses, which people are `employee`, `MAX_STAFF_COMPUTERS = 16`.

## Review Focus

1. **Case and spelling tricks on the staff paths.** Windows serves `/STAFF/login.html` from `web/staff/` because its file system ignores case, and `/staff` or `/api/staff` without a trailing slash, or `/x/../staff/login.html`, must not slip past the prefix check. Expected: all answer exactly like a missing page (Task 4, E2E `staff_hidden_case_and_path_tricks`).
2. **IPv4-mapped addresses.** A dual-stack socket can report `::ffff:10.0.0.5` for a listed `10.0.0.5`. Expected: allowed (Task 4, unit `staff_computer_ipv4_mapped_matches`).
3. **Duplicate emails in the university list**, including an employee row's email repeating another row's. Expected: a load error naming the line, not a silent second owner of one email (Task 1, unit `uni_duplicate_email_refused`).
4. **An admin-added person later tries self sign-up**, by ID or with the personal email. Expected: refused (ID → `AlreadyRegistered`; non-university email → `NotUniversityEmail`), and the existing account is untouched (Task 2, unit `register_after_add_person_refused`).
5. **Mixed-case personal email in `add-person`** (`Rahim.Driver@Example.com`). Expected: stored lower-case, and login works whatever case is typed (Task 2, unit `add_person_email_lowercased`).

---

### Task 1: Employee role and university list

**Files:**
- Modify: `src/auth/users.h` (enum, `UniEntry` comment), `src/auth/users.cpp` (`roleName`, `roleFromName`, `uniLoadCsv`)
- Test: `tests/test_employee.cpp` (new; unit sources are globbed)

**Interfaces:**
- Produces: `enum class Role { Student, Teacher, Staff, Employee };` — `roleName(Role::Employee)` = `"employee"`; `roleFromName("employee", r)` true. `UniEntry::email` may be `""` only when `role == Role::Employee`.

- [ ] **Step 1: Write the failing tests** in `tests/test_employee.cpp`:
  - `employee_role_round_trips`: `roleName(Role::Employee) == "employee"`; `roleFromName("employee", r)` true and `r == Role::Employee`.
  - `uni_employee_blank_email_loads`: CSV row `3000000000000001,,employee,Rahim Driver,Transport` loads; entry email `""`, role Employee.
  - `uni_blank_email_on_student_refused`: `9990000000009999,,student,X,CSE` → false, `err` contains `line 2` (header is line 1).
  - `uni_duplicate_email_refused`: two rows with `a@uni.example` → false, err names the second row's line and contains `duplicate email`.
  - `users_file_with_employee_round_trips`: `saveUsers` then `loadUsers` keeps `Role::Employee`.
- [ ] **Step 2: Run** `build/unit_tests.exe employee` and `build/unit_tests.exe uni_` — Expected: compile error (no `Role::Employee`), then FAIL.
- [ ] **Step 3: Implement.** Add `Employee` to the enum and both name functions. In `uniLoadCsv`: accept `f[1].empty()` only for `Role::Employee`, otherwise `validEmail`; refuse a non-empty email already used by an earlier row (`: duplicate email`); error text for a bad row becomes `expected id,email,student|teacher|employee,name,department`. Check `users.cpp`'s users-file loader accepts `employee` (it uses `roleFromName`).
- [ ] **Step 4: Run** `build/unit_tests.exe` — Expected: all pass (existing count + 5).
- [ ] **Step 5: Commit** `feat(auth): employee role; university list allows blank email for employees, refuses duplicate emails`.

### Task 2: Employee sign-up rules, `addPerson`, admin `add-person`

**Files:**
- Modify: `src/auth/auth.h`, `src/auth/auth.cpp`, `src/console/cafeteria_admin.cpp`, `tools/run_admin_test.cmake`
- Test: `tests/test_employee.cpp`, `tests/e2e/e2e_section12.cpp`

**Interfaces:**
- Consumes: Task 1 `Role::Employee`, blank `UniEntry::email`.
- Produces:
  - `AuthResult::NoUniversityEmail` (message = the exact refusal copy) and `AuthResult::NotEligible` (message `That ID is not a university staff member without a university email.`).
  - `AuthResult addPerson(AuthState& a, const std::string& universityId, const std::string& email, const std::string& password);` — account login = lower-cased email, role/name/department copied from the list row, `verified = true`.
  - CLI: `cafeteria_admin [--data DIR] [--config DIR] add-person <university ID> <email>` (config default `config`; reads `<config>/university_list.csv`).

- [ ] **Step 1: Write the failing tests** (fixture: `authInit` with domain `uni.example`, a university list holding a student row, an employee row with email `guard1@uni.example`, and a blank-email employee row `3000000000000001`):
  - `employee_self_signup_with_university_email`: `registerStart` for the employee row → `Ok`, code queued; after `registerVerify`, `login` → `Ok`, user role `Employee`.
  - `blank_email_row_cannot_self_register`: `registerStart("anyone@uni.example", "3000000000000001", ...)` → `NoUniversityEmail`, no notification queued.
  - `add_person_creates_active_account`: `addPerson(a, "3000000000000001", "rahim@example.com", "driverpass1")` → `Ok`; `login` with it → `Ok`; name/department from the list.
  - `add_person_refusals`: student row ID → `NotEligible`; employee row that has an email → `NotEligible`; unknown ID → `IdNotListed`; email used by another account or another list row → `AlreadyRegistered`; same ID added twice → `AlreadyRegistered`; `"short"` → `WeakPassword`; `"not an email"` → `InvalidInput`.
  - `add_person_email_lowercased` (Review Focus 5): `Rahim.Driver@Example.com` stored as `rahim.driver@example.com`; `login("RAHIM.DRIVER@example.com", ...)` → `Ok`.
  - `register_after_add_person_refused` (Review Focus 4): after `addPerson`, `registerStart` with the uni-domain email and that ID → `NoUniversityEmail`; with `rahim@example.com` → `NotUniversityEmail`; the account's password hash is unchanged.
  - `employee_same_limit_as_teacher`: a `CafeFixture` reservation by an employee login is limited by `perUserLimit` exactly like a teacher's (reserve 2 → `Ok`, a 3rd portion → `UserLimit`).
- [ ] **Step 2: Run** `build/unit_tests.exe person` / `employee` — Expected: compile error, then FAIL.
- [ ] **Step 3: Implement** `addPerson` beside `addStaff` (hash with `hashPassword`, `CryptoError` on empty hash). In `registerStart`, after the ID lookup, return `NoUniversityEmail` when the entry's email is empty, before the email-mismatch check. Admin tool: parse `--config`, load the university list with `uniLoadCsv` only for `add-person`, read the password twice as `add-staff` does, print `University staff account '<email>' created.`; usage line lists both commands.
- [ ] **Step 4: Extend `tools/run_admin_test.cmake`**: write `${WORK}/config/university_list.csv` with a blank-email employee row and a student row; `add-person` succeeds and `users.txt` contains `|3000000000000001|employee|` and no plain password; refused for the student ID, a duplicate, a short password, and while `server.lock` holds a live PID (same PID-4 trick as `add-staff`).
- [ ] **Step 5: Write the failing E2E checks.** The E2E university list gains `3000000000000002,guard1@uni.example,employee,Sample Guard,Security` and blank-email `3000000000000001,,employee,Rahim Driver,Transport`; next to the existing `addStaff` call before the first start, call `addPerson(a, "3000000000000001", "rahim@example.com", PW)`. (Write these before Step 3's code, so they are seen failing: the server refuses the list row and `addPerson` does not exist yet.)
  - `employee: registers with a university email and reserves` (register → code from outbox → verify → reserve 1 Tehari → 200; `/api/me` role `employee`).
  - `employee: blank-email row refused with the office message` (`/api/register` with that ID → 400, body contains `Ask the cafeteria office`).
  - `employee: admin-added account logs in, reserves, gets the confirmation email` (outbox file for `rahim@example.com` contains `R-`).
- [ ] **Step 6: Run** `ctest --test-dir build` — Expected: 6/6 (unit, admin_tool and E2E all pass).
- [ ] **Step 7: Commit** `feat(auth,admin): employee sign-up rules and admin add-person for staff without a university email`.

### Task 3: Employees in the pages

**Files:**
- Modify: `web/js/api.js`, `web/js/menu.js` (badge/profile), `web/js/account.js`, `web/index.html`, `web/login.html`, `web/register.html` (copy), `demo/config/university_list.csv` (sample employee rows), `tests/e2e/e2e_section12.cpp`

**Interfaces:**
- Consumes: `/api/me` `role` = `"employee"` (Task 2).
- Produces: `roleLabel(role)` in `web/js/api.js` returning `Student` / `Teacher` / `University staff`, used by `menu.js` and `account.js`.

- [ ] **Step 1: Write the failing E2E check** `pages: copy names university staff` — the bodies of `/`, `/login.html` and `/register.html` contain `university staff`, and `/js/api.js` contains `University staff`.
- [ ] **Step 2: Run** the E2E binary (absolute paths) — Expected: that check FAILS.
- [ ] **Step 3: Implement** `roleLabel`; `menu.js` profile shows `Student ID` only for students and `Department` for everyone else; `account.js` uses `roleLabel` and shows Student ID only for students. Copy: "students and teachers" → "students, teachers and university staff" on index, login, register. Demo list gains the two sample employee rows from Task 2.
- [ ] **Step 4: Run** `ctest --test-dir build` — Expected: 6/6.
- [ ] **Step 5: Browser check** on the demo server: for `guard1@uni.example` the menu badge and My account say "University staff" and show the department, no Student ID.
- [ ] **Step 6: Commit** `feat(web): university staff in the page copy, menu badge and My account`.

### Task 4: Hidden staff terminal

**Files:**
- Create: `src/auth/staff_computers.h`, `src/auth/staff_computers.cpp`
- Modify: `src/server/app.h` (`StaffComputers staffComputers;` in `AppState`), `src/server/main.cpp` (load + notice), `src/server/routes.cpp` (pre-routing), `web/index.html`, `web/login.html` (remove staff links), `tests/e2e/e2e_section12.cpp`, `config/staff_computers.txt` (new, comments + `[CONFIRM]` line only, so the list is empty), `demo/config/staff_computers.txt` (empty list)
- Test: `tests/test_staff_computers.cpp`

**Interfaces:**
- Produces:
  ```cpp
  const int MAX_STAFF_COMPUTERS = 16;  // [CONFIRM]
  struct StaffComputers { std::string addr[MAX_STAFF_COMPUTERS]; int count; };
  void staffComputersInit(StaffComputers& sc);
  // Missing file = empty list. '#' lines and blank lines skipped. A malformed address
  // or more than MAX_STAFF_COMPUTERS entries -> false, err = "<path> line <n>: <reason>".
  bool staffComputersLoad(StaffComputers& sc, const std::string& path, std::string& err);
  // Empty list -> true. Otherwise exact match; "::ffff:a.b.c.d" also matches "a.b.c.d".
  bool staffComputerAllowed(const StaffComputers& sc, const std::string& remoteAddr);
  // True for any path that reaches the staff terminal, compared case-insensitively:
  // "/staff", "/staff/...", "/api/staff", "/api/staff/...".
  bool isStaffPath(const std::string& path);
  ```
  Valid address: IPv4 dotted quad, each part 0–255 without leading zeros; or IPv6 of hex digits and `:` with at least two `:` (and an optional dotted-quad tail).

- [ ] **Step 1: Write the failing unit tests** (`tests/test_staff_computers.cpp`): `staff_computers_missing_file_is_empty`; `staff_computers_comments_and_blanks`; `staff_computers_bad_line_refused` (`192.168.1.300` on line 3 → err has `line 3`); `staff_computers_too_many_refused` (17 lines); `staff_computer_empty_list_allows_all`; `staff_computer_exact_match` (`10.0.0.5` listed → `10.0.0.5` true, `10.0.0.50` false, `127.0.0.1` false); `staff_computer_ipv4_mapped_matches` (Review Focus 2); `is_staff_path_cases` (`/staff/login.html`, `/STAFF/portal.html`, `/staff`, `/api/staff`, `/api/Staff/overview` true; `/staffing.html`, `/api/me`, `/menu.html` false).
- [ ] **Step 2: Run** `build/unit_tests.exe staff_` — Expected: compile error, then FAIL.
- [ ] **Step 3: Implement** the module (fixed array; add `src/auth/staff_computers.cpp` to the server's and unit tests' sources if they are not globbed).
- [ ] **Step 4: Write the failing E2E block** at the end of the suite. The existing staff checks need the test computer allowed, so the E2E config's first `staff_computers.txt` lists `127.0.0.1` and `::1`; this block restarts the server with `10.20.30.40` as the only entry, then with an empty file. (The spec's "started with a list that does not list 127.0.0.1" is this block; the phase order does not change what is proven.)
  - `staff hidden: pages and APIs answer like a missing page` — for `/staff/login.html`, `/staff/portal.html`, `/api/staff/overview` (GET) and `/api/staff/login` (POST): status and body equal those of `/no-such-page.html` (GET) and `/api/no-such-route` (POST).
  - `staff_hidden_case_and_path_tricks` (Review Focus 1): `/STAFF/login.html`, `/Staff/portal.html`, `/staff`, `/api/staff`, `/x/../staff/login.html` → same as `/no-such-page.html`.
  - `staff hidden: no public page links to /staff/` — bodies of `/`, `/login.html`, `/register.html` (and `/menu.html`, `/reservations.html`, `/account.html` fetched with a student session) contain no `/staff/`.
  - `staff hidden: empty list prints the notice and the page opens` — restart with an empty file; `server.log` contains the exact notice; `/staff/login.html` → 200.
- [ ] **Step 5: Run** the E2E binary — Expected: the new checks FAIL against the current server.
- [ ] **Step 6: Implement.** `main.cpp`: load `<config>/staff_computers.txt` after the university list (exit 1 with the error on a bad file), print the notice when `count == 0`. `routes.cpp` pre-routing handler, **first** check: `if (isStaffPath(req.path) && !staffComputerAllowed(app.staffComputers, req.remote_addr))` → `res.status = 404` with no body (what httplib sends for an unknown path; the E2E comparison is the proof — if it differs, copy what httplib sends), add `Connection: close` when the request carries a body (it is left unread), return `Handled`. Remove the `Staff portal` links (top bar and footer on `index.html`, top bar on `login.html`); footer becomes `For BAUSTK students, teachers and university staff`.
- [ ] **Step 7: Run** `ctest --test-dir build` — Expected: 6/6; E2E all pass.
- [ ] **Step 8: Commit** `feat(server): staff terminal answers like a missing page except from listed cafeteria computers; staff links removed`.

### Task 5: Eggshell theme

**Files:**
- Modify: `web/css/app.css`, `tools/contrast_check.py`, `src/server/routes.cpp` (`/api/public/info` gains `"open"`), `web/js/api.js` (`fillAnnouncement`), `web/index.html`, `web/menu.html`, `web/reservations.html`, `web/account.html` (bar markup), `tests/e2e/e2e_section12.cpp`

**Interfaces:**
- Consumes: `/api/public/info` (`now`, `cutoff`, …).
- Produces: `/api/public/info` field `open` (bool, `beforeMinuteOfDay(caf, cutoffMinute)`); `fillAnnouncement(id)` in `api.js` that sets the bar's `textContent` from it; bar markup `<p class="announce" id="announce" role="note"></p>` as the first element of `<body>` after the skip link.

- [ ] **Step 1: Write the failing checks.** E2E: `theme: public info says whether orders are open` (`"open":true` at 08:00, `"open":false` once the clock is past the cutoff); `theme: no dark-mode stylesheet` (`/css/app.css` has no `prefers-color-scheme`, contains `color-scheme: light`, `--page: #EFE9D9` and `--bar: #FFFFFF`). Contrast script: replace `PAIRS` with the spec's Part 3 pairs (19 ratios in its table) and no `dark:` entries. The script checks literal colour pairs, so it passes as soon as it is edited; the RED for this task is the two E2E checks.
- [ ] **Step 2: Run** the E2E binary — Expected: the two theme checks FAIL.
- [ ] **Step 3: Implement the tokens** exactly as the spec's colour table; delete the dark block and the graphite-bar/steel/concrete tokens (graphite stays only as `--ink`); `--radius: 0`; top bar white with `border-bottom: 3px solid var(--accent)`, hero panel white; `--line` (`#C8CDD2`) for decorative lines only; inputs, selects, textareas and outline/secondary buttons get `1px solid var(--edge)` on white; green text uses `--accent-text` (`#0F5C3A`), never `--accent`. No dark colour as a background anywhere (`.topbar`, `.footer`, buttons included: the former graphite button becomes a white outline button with a green edge).
- [ ] **Step 4: Implement the structure:** `.announce` bar (deep green, white, uppercase 0.75rem, 0.14em); line grid of dish tiles (`gap: 0`, light stainless lines; grid draws left/top edges, tiles right/bottom); uppercase labels (0.75rem, 0.14em) for category, badges, table headings, nav; dish picture spotlight `radial-gradient(ellipse at 50% 40%, #FFFFFF 0%, #EDE7D7 60%, #E0D9C6 100%)`, 60% opacity when sold out; a 2px deep-green rule under `h1`. Add `open` to `/api/public/info`; `fillAnnouncement` on the four pages.
- [ ] **Step 5: Run** `python tools/contrast_check.py` (Expected: all pairs pass, 0 failed) and `ctest --test-dir build` (Expected: 6/6).
- [ ] **Step 6: Browser check** every page (landing, login, register, menu, reservations, account, staff login, staff portal) at desktop and 375 px, once with `colorScheme: dark` emulated: page stays eggshell, inputs light, no sideways scroll (`scrollWidth == innerWidth`), bar text matches open/closed.
- [ ] **Step 7: Commit** `feat(web): eggshell theme, one theme for everyone; white top bar, announcement bar, line grid, square corners`.

### Task 6: Docs, final review, clean rebuild

**Files:**
- Modify: `README.md` (pages table, "students, teachers and university staff", `add-person`, `staff_computers.txt` and the proxy limit, theme sentence), `demo/README.md` (staff open `/staff/login.html` directly; employee demo login), `docs/testing/test-report.md` (fresh outputs, new rows), `docs/accessibility-review.md` (theme re-check)

- [ ] **Step 1:** Update the docs; regenerate `docs/testing/unit_tests_output.txt`, `e2e_section12_output.txt`, `ctest_summary.txt`.
- [ ] **Step 2:** Fresh reviewer (most capable model) on the range from this plan's commit to HEAD with this plan's Review Focus and the ledger's rulings; re-grade; one fix pass, each fix RED→GREEN.
- [ ] **Step 3:** Clean clone into a new scratch folder, configure, build (0 warnings), `ctest` 6/6.
- [ ] **Step 4: Commit** `docs: university staff, hidden staff terminal, eggshell theme; test report re-run`.
