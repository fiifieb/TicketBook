# 🎟️ TicketBook

TicketBook is a multithreaded ticket reservation core written in **C**, with optional **RISC‑V assembly** fast paths and an in-memory DB stub used by tests.

## Build

From a clean checkout:

```bash
make
```

This builds all test binaries:

- `tests/test_hashtable`
- `tests/test_db_interface`
- `tests/test_reservation`

On `riscv64`, `src/riscv_inline.s` is compiled and linked automatically.

## Test

Run all tests:

```bash
make test
```

Run one test:

```bash
make test_hashtable
make test_db_interface
make test_reservation
```

## Clean

```bash
make clean
```
