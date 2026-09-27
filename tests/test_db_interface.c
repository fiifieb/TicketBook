// Unit tests for DB stub interface
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <postgresql/libpq-fe.h>

#include "db_interface.h"
#include "db_config.h"

static int g_postgres_enabled = 0;

static void maybe_enable_postgres_and_bootstrap(void)
{
    const char *test_dsn = getenv("TB_TEST_POSTGRES_DSN");
    if (!test_dsn || test_dsn[0] == '\0')
        return;

    assert(setenv(TB_POSTGRES_DSN_ENV, test_dsn, 1) == 0);

    PGconn *conn = PQconnectdb(test_dsn);
    assert(conn != NULL);
    assert(PQstatus(conn) == CONNECTION_OK);

    PGresult *res = PQexec(
        conn,
        "CREATE SEQUENCE IF NOT EXISTS orders_order_id_seq START 1;"
        "CREATE TABLE IF NOT EXISTS seats ("
        "event_id TEXT NOT NULL,"
        "seat_id TEXT NOT NULL,"
        "price_cents INTEGER NOT NULL CHECK (price_cents >= 0),"
        "status TEXT NOT NULL CHECK (status IN ('AVAILABLE', 'HELD', 'SOLD', 'REFUNDED')),"
        "last_order_id TEXT,"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),"
        "PRIMARY KEY (event_id, seat_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS orders ("
        "order_id TEXT PRIMARY KEY DEFAULT ('ORD-' || nextval('orders_order_id_seq')),"
        "user_id TEXT NOT NULL,"
        "event_id TEXT NOT NULL,"
        "seat_id TEXT NOT NULL,"
        "price_cents INTEGER NOT NULL CHECK (price_cents >= 0),"
        "hold_token BYTEA NOT NULL UNIQUE,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()"
        ");"
        "CREATE TABLE IF NOT EXISTS refunds ("
        "refund_id BIGSERIAL PRIMARY KEY,"
        "user_id TEXT NOT NULL,"
        "order_id TEXT NOT NULL REFERENCES orders(order_id) ON DELETE CASCADE,"
        "amount_cents INTEGER NOT NULL CHECK (amount_cents >= 0),"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()"
        ");"
        "TRUNCATE TABLE refunds, orders, seats;");
    assert(res != NULL);
    assert(PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);

    res = PQexec(conn,
                 "INSERT INTO seats (event_id, seat_id, price_cents, status) "
                 "VALUES ('E_PRICE', 'S_PRICE', 4321, 'AVAILABLE') "
                 "ON CONFLICT (event_id, seat_id) DO UPDATE SET price_cents = EXCLUDED.price_cents, status = EXCLUDED.status");
    assert(res != NULL);
    assert(PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    PQfinish(conn);

    g_postgres_enabled = 1;
}

int main(void)
{
    maybe_enable_postgres_and_bootstrap();

    // Begin and commit a transaction
    db_txn_t *txn = db_txn_begin();
    assert(txn);
    assert(db_txn_commit(txn));

    // Create an order
    tb_byte_t tok[4] = {1,2,3,4};
    char order_id[RES_ID_LEN] = {0};
    db_txn_t *t2 = db_txn_begin();
    assert(db_order_create(t2, "U1", "E1", "A1", 1234, tok, 4, order_id) == RES_OK);
    assert(strlen(order_id) > 0);
    assert(db_seat_mark_sold(t2, "E1", "A1", order_id) == RES_OK);
    assert(db_txn_commit(t2));

    // Find by token
    char found_id[RES_ID_LEN] = {0};
    tb_money_cents_t price = 0;
    assert(db_order_find_by_token(tok, 4, found_id, &price) == RES_OK);
    assert(strcmp(found_id, order_id) == 0);
    assert(price == 1234);

    // Find by id
    char user[RES_ID_LEN] = {0}, ev[RES_ID_LEN] = {0}, seat[RES_ID_LEN] = {0};
    assert(db_order_find_by_id(order_id, user, ev, seat, &price) == RES_OK);
    assert(strcmp(user, "U1") == 0);
    assert(strcmp(ev, "E1") == 0);
    assert(strcmp(seat, "A1") == 0);
    assert(price == 1234);

    if (g_postgres_enabled)
    {
        tb_money_cents_t p = 0;
        assert(db_authoritative_price("E_PRICE", "S_PRICE", &p) == RES_OK);
        assert(p == 4321);
    }

    // Refund
    db_txn_t *t3 = db_txn_begin();
    assert(db_refund_create(t3, "U1", order_id, 1234) == RES_OK);
    assert(db_txn_commit(t3));

    printf("All DB interface tests passed.\n");
    return 0;
}
