// In-memory DB stub with optional PostgreSQL backend.

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

#include <postgresql/libpq-fe.h>

#include "db_interface.h"
#include "db_config.h"

typedef struct order_row {
    char order_id[RES_ID_LEN];
    char user_id[RES_ID_LEN];
    char event_id[RES_ID_LEN];
    char seat_id[RES_ID_LEN];
    tb_money_cents_t price;
    tb_byte_t token[RES_TOKEN_LEN];
    size_t token_len;
    struct order_row *next;
} order_row_t;

static pthread_mutex_t g_db_mtx = PTHREAD_MUTEX_INITIALIZER;
static order_row_t *g_orders = NULL;
static uint64_t g_order_seq = 1;

struct db_txn {
    bool use_postgres;
    bool closed;
    PGconn *conn;
};

static bool db_postgres_enabled(void)
{
    const char *dsn = getenv(TB_POSTGRES_DSN_ENV);
    return dsn && dsn[0] != '\0';
}

static PGconn *db_connect_postgres(void)
{
    const char *dsn = getenv(TB_POSTGRES_DSN_ENV);
    if (!dsn || dsn[0] == '\0')
        return NULL;

    PGconn *conn = PQconnectdb(dsn);
    if (!conn)
        return NULL;
    if (PQstatus(conn) != CONNECTION_OK)
    {
        PQfinish(conn);
        return NULL;
    }
    return conn;
}

static bool pg_exec_ok(PGconn *conn, const char *sql)
{
    PGresult *res = PQexec(conn, sql);
    if (!res)
        return false;
    ExecStatusType status = PQresultStatus(res);
    PQclear(res);
    return status == PGRES_COMMAND_OK;
}

static void safe_copy_id(char out[RES_ID_LEN], const char *src)
{
    if (!out || !src)
        return;
    strncpy(out, src, RES_ID_LEN - 1);
    out[RES_ID_LEN - 1] = '\0';
}

static void gen_order_id(char out[RES_ID_LEN])
{
    snprintf(out, RES_ID_LEN, "ORD-%llu", (unsigned long long)g_order_seq++);
}

static res_code_t pg_order_find_by_token(const tb_byte_t *hold_token,
                                         size_t token_len,
                                         char out_order_id[RES_ID_LEN],
                                         tb_money_cents_t *out_price)
{
    PGconn *conn = db_connect_postgres();
    if (!conn)
        return RES_DB_ERROR;

    const char *param_values[1] = {(const char *)hold_token};
    int param_lengths[1] = {(int)token_len};
    int param_formats[1] = {1};

    PGresult *res = PQexecParams(conn,
                                 "SELECT order_id, price_cents FROM orders WHERE hold_token = $1 LIMIT 1",
                                 1,
                                 NULL,
                                 param_values,
                                 param_lengths,
                                 param_formats,
                                 0);
    if (!res)
    {
        PQfinish(conn);
        return RES_DB_ERROR;
    }
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);
        PQfinish(conn);
        return RES_DB_ERROR;
    }
    if (PQntuples(res) == 0)
    {
        PQclear(res);
        PQfinish(conn);
        return RES_NOT_FOUND;
    }

    if (out_order_id)
        safe_copy_id(out_order_id, PQgetvalue(res, 0, 0));
    if (out_price)
        *out_price = (tb_money_cents_t)strtol(PQgetvalue(res, 0, 1), NULL, 10);

    PQclear(res);
    PQfinish(conn);
    return RES_OK;
}

static res_code_t pg_order_find_by_id(const char *order_id,
                                      char out_user_id[RES_ID_LEN],
                                      char out_event_id[RES_ID_LEN],
                                      char out_seat_id[RES_ID_LEN],
                                      tb_money_cents_t *out_price)
{
    PGconn *conn = db_connect_postgres();
    if (!conn)
        return RES_DB_ERROR;

    const char *param_values[1] = {order_id};
    PGresult *res = PQexecParams(conn,
                                 "SELECT user_id, event_id, seat_id, price_cents FROM orders WHERE order_id = $1 LIMIT 1",
                                 1,
                                 NULL,
                                 param_values,
                                 NULL,
                                 NULL,
                                 0);
    if (!res)
    {
        PQfinish(conn);
        return RES_DB_ERROR;
    }
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);
        PQfinish(conn);
        return RES_DB_ERROR;
    }
    if (PQntuples(res) == 0)
    {
        PQclear(res);
        PQfinish(conn);
        return RES_NOT_FOUND;
    }

    if (out_user_id)
        safe_copy_id(out_user_id, PQgetvalue(res, 0, 0));
    if (out_event_id)
        safe_copy_id(out_event_id, PQgetvalue(res, 0, 1));
    if (out_seat_id)
        safe_copy_id(out_seat_id, PQgetvalue(res, 0, 2));
    if (out_price)
        *out_price = (tb_money_cents_t)strtol(PQgetvalue(res, 0, 3), NULL, 10);

    PQclear(res);
    PQfinish(conn);
    return RES_OK;
}

db_txn_t *db_txn_begin(void)
{
    db_txn_t *txn = (db_txn_t *)calloc(1, sizeof(db_txn_t));
    if (!txn)
        return NULL;

    txn->use_postgres = db_postgres_enabled();
    if (!txn->use_postgres)
        return txn;

    txn->conn = db_connect_postgres();
    if (!txn->conn)
    {
        free(txn);
        return NULL;
    }

    if (!pg_exec_ok(txn->conn, "BEGIN"))
    {
        PQfinish(txn->conn);
        free(txn);
        return NULL;
    }

    return txn;
}

bool db_txn_commit(db_txn_t *txn)
{
    if (!txn || txn->closed)
        return false;

    bool ok = true;
    if (txn->use_postgres)
        ok = pg_exec_ok(txn->conn, "COMMIT");

    txn->closed = true;
    if (txn->conn)
        PQfinish(txn->conn);
    free(txn);
    return ok;
}

void db_txn_rollback(db_txn_t *txn)
{
    if (!txn)
        return;
    if (!txn->closed && txn->use_postgres && txn->conn)
        (void)pg_exec_ok(txn->conn, "ROLLBACK");
    txn->closed = true;
    if (txn->conn)
        PQfinish(txn->conn);
    free(txn);
}

res_code_t db_authoritative_price(const char *event_id,
                                  const char *seat_id,
                                  tb_money_cents_t *out_price)
{
    if (!db_postgres_enabled())
    {
        (void)event_id;
        (void)seat_id;
        (void)out_price;
        return RES_NOT_FOUND;
    }

    PGconn *conn = db_connect_postgres();
    if (!conn)
        return RES_DB_ERROR;

    const char *param_values[2] = {event_id, seat_id};
    PGresult *res = PQexecParams(conn,
                                 "SELECT price_cents FROM seats WHERE event_id = $1 AND seat_id = $2 LIMIT 1",
                                 2,
                                 NULL,
                                 param_values,
                                 NULL,
                                 NULL,
                                 0);
    if (!res)
    {
        PQfinish(conn);
        return RES_DB_ERROR;
    }
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);
        PQfinish(conn);
        return RES_DB_ERROR;
    }
    if (PQntuples(res) == 0)
    {
        PQclear(res);
        PQfinish(conn);
        return RES_NOT_FOUND;
    }

    if (out_price)
        *out_price = (tb_money_cents_t)strtol(PQgetvalue(res, 0, 0), NULL, 10);

    PQclear(res);
    PQfinish(conn);
    return RES_OK;
}

res_code_t db_order_find_by_token(const tb_byte_t *hold_token,
                                  size_t token_len,
                                  char out_order_id[RES_ID_LEN],
                                  tb_money_cents_t *out_price)
{
    if (!hold_token || token_len == 0)
        return RES_NOT_FOUND;

    if (db_postgres_enabled())
        return pg_order_find_by_token(hold_token, token_len, out_order_id, out_price);

    pthread_mutex_lock(&g_db_mtx);
    for (order_row_t *r = g_orders; r; r = r->next)
    {
        if (r->token_len == token_len && memcmp(r->token, hold_token, token_len) == 0)
        {
            if (out_order_id)
                safe_copy_id(out_order_id, r->order_id);
            if (out_price)
                *out_price = r->price;
            pthread_mutex_unlock(&g_db_mtx);
            return RES_OK;
        }
    }
    pthread_mutex_unlock(&g_db_mtx);
    return RES_NOT_FOUND;
}

res_code_t db_order_find_by_id(const char *order_id,
                               char out_user_id[RES_ID_LEN],
                               char out_event_id[RES_ID_LEN],
                               char out_seat_id[RES_ID_LEN],
                               tb_money_cents_t *out_price)
{
    if (!order_id)
        return RES_NOT_FOUND;

    if (db_postgres_enabled())
        return pg_order_find_by_id(order_id, out_user_id, out_event_id, out_seat_id, out_price);

    pthread_mutex_lock(&g_db_mtx);
    for (order_row_t *r = g_orders; r; r = r->next)
    {
        if (strncmp(r->order_id, order_id, RES_ID_LEN) == 0)
        {
            if (out_user_id)
                safe_copy_id(out_user_id, r->user_id);
            if (out_event_id)
                safe_copy_id(out_event_id, r->event_id);
            if (out_seat_id)
                safe_copy_id(out_seat_id, r->seat_id);
            if (out_price)
                *out_price = r->price;
            pthread_mutex_unlock(&g_db_mtx);
            return RES_OK;
        }
    }
    pthread_mutex_unlock(&g_db_mtx);
    return RES_NOT_FOUND;
}

res_code_t db_order_create(db_txn_t *txn,
                           const char *user_id,
                           const char *event_id,
                           const char *seat_id,
                           tb_money_cents_t price_cents,
                           const tb_byte_t *hold_token,
                           size_t token_len,
                           char out_order_id[RES_ID_LEN])
{
    if (!txn || !user_id || !event_id || !seat_id || !hold_token || token_len == 0)
        return RES_INTERNAL_ERR;

    if (txn->use_postgres)
    {
        char price_buf[32];
        snprintf(price_buf, sizeof(price_buf), "%d", (int)price_cents);
        const char *param_values[5] = {
            user_id, event_id, seat_id, price_buf, (const char *)hold_token
        };
        int param_lengths[5] = {0, 0, 0, 0, (int)token_len};
        int param_formats[5] = {0, 0, 0, 0, 1};

        PGresult *res = PQexecParams(
            txn->conn,
            "INSERT INTO orders (user_id, event_id, seat_id, price_cents, hold_token) "
            "VALUES ($1, $2, $3, $4, $5) "
            "RETURNING order_id",
            5,
            NULL,
            param_values,
            param_lengths,
            param_formats,
            0);
        if (!res)
            return RES_DB_ERROR;
        if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1)
        {
            PQclear(res);
            return RES_DB_ERROR;
        }
        if (out_order_id)
            safe_copy_id(out_order_id, PQgetvalue(res, 0, 0));
        PQclear(res);
        return RES_OK;
    }

    order_row_t *row = (order_row_t *)calloc(1, sizeof(order_row_t));
    if (!row)
        return RES_INTERNAL_ERR;
    safe_copy_id(row->user_id, user_id);
    safe_copy_id(row->event_id, event_id);
    safe_copy_id(row->seat_id, seat_id);
    row->price = price_cents;
    row->token_len = token_len > RES_TOKEN_LEN ? RES_TOKEN_LEN : token_len;
    memcpy(row->token, hold_token, row->token_len);
    gen_order_id(row->order_id);

    pthread_mutex_lock(&g_db_mtx);
    row->next = g_orders;
    g_orders = row;
    if (out_order_id)
        safe_copy_id(out_order_id, row->order_id);
    pthread_mutex_unlock(&g_db_mtx);

    return RES_OK;
}

res_code_t db_seat_mark_sold(db_txn_t *txn,
                             const char *event_id,
                             const char *seat_id,
                             const char *order_id)
{
    if (!txn)
        return RES_INTERNAL_ERR;
    if (!txn->use_postgres)
        return RES_OK;

    const char *param_values[3] = {event_id, seat_id, order_id};
    PGresult *res = PQexecParams(
        txn->conn,
        "INSERT INTO seats (event_id, seat_id, price_cents, status, last_order_id) "
        "VALUES ($1, $2, 0, 'SOLD', $3) "
        "ON CONFLICT (event_id, seat_id) DO UPDATE SET "
        "status = 'SOLD', last_order_id = EXCLUDED.last_order_id, updated_at = NOW()",
        3,
        NULL,
        param_values,
        NULL,
        NULL,
        0);
    if (!res)
        return RES_DB_ERROR;
    ExecStatusType status = PQresultStatus(res);
    PQclear(res);
    return status == PGRES_COMMAND_OK ? RES_OK : RES_DB_ERROR;
}

res_code_t db_refund_create(db_txn_t *txn,
                            const char *user_id,
                            const char *order_id,
                            tb_money_cents_t amount_cents)
{
    if (!txn || !user_id || !order_id)
        return RES_INTERNAL_ERR;

    if (!txn->use_postgres)
        return RES_OK;

    char amount_buf[32];
    snprintf(amount_buf, sizeof(amount_buf), "%d", (int)amount_cents);
    const char *param_values[3] = {user_id, order_id, amount_buf};
    PGresult *res = PQexecParams(
        txn->conn,
        "INSERT INTO refunds (user_id, order_id, amount_cents) VALUES ($1, $2, $3)",
        3,
        NULL,
        param_values,
        NULL,
        NULL,
        0);
    if (!res)
        return RES_DB_ERROR;
    ExecStatusType status = PQresultStatus(res);
    PQclear(res);
    return status == PGRES_COMMAND_OK ? RES_OK : RES_DB_ERROR;
}
