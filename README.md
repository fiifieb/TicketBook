# 🎟️ TicketBook

TicketBook is a multithreaded ticket reservation core written in **C**, with optional **RISC‑V assembly** fast paths and an in-memory DB stub used by tests.

## PostgreSQL support

`db_interface` can run in two modes:

- **default**: in-memory stub (no database needed)
- **PostgreSQL**: set `TB_POSTGRES_DSN` to enable database-backed operations

Apply schema:

```bash
psql "$TB_POSTGRES_DSN" -f sql/schema.sql
```

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

Run DB tests against PostgreSQL:

```bash
export TB_TEST_POSTGRES_DSN="postgresql://..."
make test_db_interface
```

## Clean

```bash
make clean
```
