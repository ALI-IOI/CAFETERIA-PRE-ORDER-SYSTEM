# University Cafeteria Food Pre-Order System (BAUSTK, DSA I)

C++ web system whose reservation logic runs on a hand-built array, linked lists,
circular queues and an array-based stack. See `docs/proposal/` for the proposal,
`docs/architecture/ADR-001-server-architecture.md` for the design and
`docs/superpowers/plans/` for the implementation plan.

## Build and test

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```
