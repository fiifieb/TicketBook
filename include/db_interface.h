#pragma once
#include <stdbool.h>
#include <stddef.h>

// Pull in shared primitives (tb_byte_t, tb_epoch_t, tb_money_cents_t) and sizes
#include "types.h"
#include "reservation.h"  // for RES_ID_LEN and res_code_t

#ifdef __cplusplus
extern "C" {
#endif

// -------------------------------
// DB transaction wrapper
// -------------------------------
typedef struct db_txn db_txn_t; // Opaque to callers

// Begin a new DB transaction. Returns NULL on failure.
db_txn_t* db_txn_begin(void);

// Commit. Returns true on success, false on failure. On failure, transaction is invalidated.
bool db_txn_commit(db_txn_t* txn);

// Rollback and free. Safe to call with NULL (no-op).
void db_txn_rollback(db_txn_t* txn);

// -------------------------------
// Authoritative price & idempotency helpers
// -------------------------------

// Fetch authoritative price for a seat (in cents) from the DB.
// Returns RES_OK and sets *out_price on success; RES_NOT_FOUND if seat/event unknown; RES_DB_ERROR on DB errors.
res_code_t db_authoritative_price(const char* event_id,
                                  const char* seat_id,
                                  tb_money_cents_t* out_price);

// Look up an existing order by hold token (idempotency). If found, fills order_id and price_cents.
// Returns RES_OK if found, RES_NOT_FOUND if no order for this token, RES_DB_ERROR on errors.
res_code_t db_order_find_by_token(const tb_byte_t* hold_token,
                                  size_t token_len,
                                  char out_order_id[RES_ID_LEN],
                                  tb_money_cents_t* out_price);

// Look up an existing order by order_id. If found, fills user_id, event_id, seat_id, and price_cents.
// Returns RES_OK if found, RES_NOT_FOUND if unknown, RES_DB_ERROR on errors.
res_code_t db_order_find_by_id(const char* order_id,
                               char out_user_id[RES_ID_LEN],
                               char out_event_id[RES_ID_LEN],
                               char out_seat_id[RES_ID_LEN],
                               tb_money_cents_t* out_price);

// -------------------------------
// Create order & persist seat state (within a transaction)
// -------------------------------

// Create an order row. Returns RES_OK and writes order_id on success.
res_code_t db_order_create(db_txn_t* txn,
                           const char* user_id,
                           const char* event_id,
                           const char* seat_id,
                           tb_money_cents_t price_cents,
                           const tb_byte_t* hold_token,
                           size_t token_len,
                           char out_order_id[RES_ID_LEN]);

// Mark the seat as sold and link to order. Call within same txn as db_order_create.
res_code_t db_seat_mark_sold(db_txn_t* txn,
                             const char* event_id,
                             const char* seat_id,
                             const char* order_id);

// -------------------------------
// Refunds (optional for phase 1)
// -------------------------------

// Record a refund and flip order/seat state per policy.
res_code_t db_refund_create(db_txn_t* txn,
                            const char* user_id,
                            const char* order_id,
                            tb_money_cents_t amount_cents);

#ifdef __cplusplus
}
#endif