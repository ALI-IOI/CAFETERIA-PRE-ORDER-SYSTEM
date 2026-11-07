# Live demo on GitHub Pages

GitHub Pages serves files, not programs, so the live demo runs the cafeteria's C++ server **in the
visitor's browser**: the same code as `build/cafeteria_server` (the hand-built data structures, the
rules in `src/core`, the text-file storage, Argon2 hashing with libsodium and every route in
`src/server/routes.cpp`) compiled to WebAssembly with Emscripten.

| Terminal | Who logs in | Live page |
|---|---|---|
| Public login | Students, teachers and university staff | https://ali-ioi.github.io/CAFETERIA-PRE-ORDER-SYSTEM/login.html |
| Staff login | Cafeteria staff | https://ali-ioi.github.io/CAFETERIA-PRE-ORDER-SYSTEM/staff/login.html |

The demo accounts are listed on both login pages (and in `demo/README.md`).

## How it works

| File | Part |
|---|---|
| `src/wasm/demo_main.cpp` | Starts the server like `src/server/main.cpp` does and answers one request at a time (`cafe_request`); the minute timer is `cafe_tick` |
| `src/wasm/httplib.h` | Stand-in for cpp-httplib with only what `routes.cpp` uses, without sockets |
| `web-demo/cafe-worker.js` | Runs the WebAssembly server in a worker shared by all open tabs; keeps `data/` in IndexedDB |
| `web-demo/demo.js` | Loaded first on every page: sends the pages' `/api/` requests to the worker, keeps the session cookies, adds the demo bar and the demo accounts |
| `web-demo/wasm.cmake`, `web-demo/make_site.cmake` | The build and the site: a copy of `web/` with links under `/CAFETERIA-PRE-ORDER-SYSTEM/` |

Nothing in `web/` or the server code changes for the demo.

Differences from running the real server, all on purpose:

- **One private cafeteria per browser.** Its data files live in that browser (IndexedDB), so visitors
  do not see each other's orders. **Reset demo** starts again.
- **Demo clock.** It starts at 08:30 on the first visit and then moves with real time; **+1 hour**
  moves it on (the `/api/dev/advance` route of `--fake-clock`).
- **Emails** are written to `data/outbox` as in the local demo; the **Email inbox** button shows them,
  including the 6-digit code for registering.
- **Sessions** are saved in `data/demo_sessions.txt`, because the browser restarts the program on
  every page load. The staff terminal and the public pages keep separate cookies, so a student tab
  and a staff tab can be open side by side.
- **The staff terminal is reachable.** The browser counts as "this computer", the address
  `config/staff_computers.txt` allows while it is empty (`demo/config` is used).

## Build

Needs the Emscripten SDK and libsodium built with it (tested: Emscripten 7.0.0, libsodium 1.0.22).

libsodium, from the signed release tarball at https://download.libsodium.org/libsodium/releases/
(in a POSIX shell with make; `emcc`, `emar`, `emranlib` and `wasm-ld` on the PATH):

```bash
./configure --host=wasm32-unknown-emscripten --prefix=$HOME/sodium-wasm --disable-shared --enable-static \
  --disable-ssp --disable-asm --without-pthreads CC=emcc LD=wasm-ld AR=emar RANLIB=emranlib CFLAGS=-O2
make && make install
```

The demo:

```bash
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release -DSODIUM_WASM=$HOME/sodium-wasm
cmake --build build-wasm --target demo_site
```

The site is in `build-wasm/site`. To try it, serve the folder that contains it under the name
`CAFETERIA-PRE-ORDER-SYSTEM` (the links start with that path), for example copy it to
`serve/CAFETERIA-PRE-ORDER-SYSTEM` and run `python -m http.server --directory serve`, then open
http://localhost:8000/CAFETERIA-PRE-ORDER-SYSTEM/.

## Publish

The live site is the `gh-pages` branch: its files are the contents of `build-wasm/site`.
