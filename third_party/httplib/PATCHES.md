# Local patch to cpp-httplib v0.60.1

**What:** five read-only `thread_local` objects in `httplib.h` became `static const`
(search for `PATCHED`):

| Line (approx.) | Object | Used for |
|---|---|---|
| 5878 | `std::regex` | file-extension lookup when serving static files |
| 5950 | `case_ignore::unordered_set<std::string>` | headers not allowed in chunked trailers |
| 8255 / 8258 | `std::regex` | parsing the HTTP status line (every client response) |
| 11571 | `std::regex` | detecting an existing query string |
| 13060 | `std::set<std::string>` | built-in HTTP method names |

The `thread_local std::mt19937` random generators were left unchanged: they have no
destructor and keep per-thread state. The remaining `thread_local`s sit in TLS-library
code (OpenSSL / mbedTLS / wolfSSL) that this build does not compile.

**Why:** in this statically linked MinGW-w64 build (GCC 13.1, `-static`), destructors of
`thread_local` objects can run on already-freed memory when a thread exits. The
process then crashes at random (exit code `0xC0000374`, heap corruption). It showed up as
intermittent E2E failures and would also hit the server's worker threads at shutdown.
A gdb backtrace showed `~basic_regex` called from `run_dtor_list` / `tls_callback`
on `0xfeeefeee...` (freed) memory.

**Why it is safe:** all five objects are only read after initialisation. C++11 makes
function-local `static` initialisation thread-safe, and calling `const` member
functions or `std::regex_search` / `regex_match` with a `const std::regex` from several
threads at once is race-free.

**Test:** `tests/stress/thread_exit_stress.cpp` (CTest `thread_exit_stress`) runs 640
short-lived client threads against an in-process server. Before the patch it crashed
5 times out of 5; after the patch it must pass every time.

When upgrading httplib, re-apply this patch (or check whether upstream has changed these
objects) and run the stress test.
