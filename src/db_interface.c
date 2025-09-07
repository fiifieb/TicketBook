// Simple in-memory stub DB implementation for development/tests.
// Thread-safe via a single global mutex. Not suitable for production.

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include "db_interface.h"

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

struct db_txn { int dummy; };

static void gen_order_id(char out[RES_ID_LEN])
{
    // Simple monotonic counter id: ORD-<seq>
    snprintf(out, RES_ID_LEN, "ORD-%llu", (unsigned long long)g_order_seq++);
}

db_txn_t* db_txn_begin(void)
{
    db_txn_t *t = (db_txn_t*)malloc(sizeof(db_txn_t));
    return t;
}

bool db_txn_commit(db_txn_t* txn)
{
    free(txn);
    return true;
}

void db_txn_rollback(db_txn_t* txn)
{
    free(txn);
}

res_code_t db_authoritative_price(const char* event_id,
                                  const char* seat_id,
                                  tb_money_cents_t* out_price)
{
    // No backing store for prices in this stub → say not found so caller keeps in-memory price
    (void)event_id; (void)seat_id; (void)out_price;
    return RES_NOT_FOUND;
}

res_code_t db_order_find_by_token(const tb_byte_t* hold_token,
                                  size_t token_len,
                                  char out_order_id[RES_ID_LEN],
                                  tb_money_cents_t* out_price)
{
    if (!hold_token || token_len == 0)
        return RES_NOT_FOUND;
    pthread_mutex_lock(&g_db_mtx);
    for (order_row_t *r = g_orders; r; r = r->next)
    {
        if (r->token_len == token_len && memcmp(r->token, hold_token, token_len) == 0)
        {
            if (out_order_id) strncpy(out_order_id, r->order_id, RES_ID_LEN - 1);
            if (out_price) *out_price = r->price;
            pthread_mutex_unlock(&g_db_mtx);
            return RES_OK;
        }
    }
    pthread_mutex_unlock(&g_db_mtx);
    return RES_NOT_FOUND;
}

res_code_t db_order_find_by_id(const char* order_id,
                               char out_user_id[RES_ID_LEN],
                               char out_event_id[RES_ID_LEN],
                               char out_seat_id[RES_ID_LEN],
                               tb_money_cents_t* out_price)
{
    if (!order_id)
        return RES_NOT_FOUND;
    pthread_mutex_lock(&g_db_mtx);
    for (order_row_t *r = g_orders; r; r = r->next)
    {
        if (strncmp(r->order_id, order_id, RES_ID_LEN) == 0)
        {
            if (out_user_id) strncpy(out_user_id, r->user_id, RES_ID_LEN - 1);
            if (out_event_id) strncpy(out_event_id, r->event_id, RES_ID_LEN - 1);
            if (out_seat_id) strncpy(out_seat_id, r->seat_id, RES_ID_LEN - 1);
            if (out_price) *out_price = r->price;
            pthread_mutex_unlock(&g_db_mtx);
            return RES_OK;
        }
    }
    pthread_mutex_unlock(&g_db_mtx);
    return RES_NOT_FOUND;
}

res_code_t db_order_create(db_txn_t* txn,
                           const char* user_id,
                           const char* event_id,
                           const char* seat_id,
                           tb_money_cents_t price_cents,
                           const tb_byte_t* hold_token,
                           size_t token_len,
                           char out_order_id[RES_ID_LEN])
{
    (void)txn;
    if (!user_id || !event_id || !seat_id || !hold_token || token_len == 0)
        return RES_INTERNAL_ERR;

    order_row_t *row = (order_row_t*)calloc(1, sizeof(order_row_t));
    if (!row)
        return RES_INTERNAL_ERR;
    strncpy(row->user_id, user_id, RES_ID_LEN - 1);
    strncpy(row->event_id, event_id, RES_ID_LEN - 1);
    strncpy(row->seat_id, seat_id, RES_ID_LEN - 1);
    row->price = price_cents;
    row->token_len = token_len > RES_TOKEN_LEN ? RES_TOKEN_LEN : token_len;
    memcpy(row->token, hold_token, row->token_len);
    gen_order_id(row->order_id);

    pthread_mutex_lock(&g_db_mtx);
    row->next = g_orders;
    g_orders = row;
    if (out_order_id) strncpy(out_order_id, row->order_id, RES_ID_LEN - 1);
    pthread_mutex_unlock(&g_db_mtx);

    return RES_OK;
}

res_code_t db_seat_mark_sold(db_txn_t* txn,
                             const char* event_id,
                             const char* seat_id,
                             const char* order_id)
{
    (void)txn; (void)event_id; (void)seat_id; (void)order_id;
    // Nothing to do in stub
    return RES_OK;
}

res_code_t db_refund_create(db_txn_t* txn,
                            const char* user_id,
                            const char* order_id,
                            tb_money_cents_t amount_cents)
{
    (void)txn; (void)user_id; (void)order_id; (void)amount_cents;
    // Accept refund in stub
    return RES_OK;
}
