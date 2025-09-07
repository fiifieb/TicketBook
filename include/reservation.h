// Public reservation API and related types

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "types.h" // seat_t, seat_status_t and TB_* sizes

#ifdef __cplusplus
extern "C" {
#endif

// Expose consistent fixed sizes used across the reservation API
#define RES_ID_LEN     TB_ID_LEN
#define RES_TOKEN_LEN  TB_TOKEN_LEN

// Result / error codes for reservation operations
typedef enum {
    RES_OK = 0,
    RES_NOT_FOUND,
    RES_ALREADY_SOLD,
    RES_HELD_BY_OTHER,
    RES_HOLD_EXISTS_SAME_USER,
    RES_INVALID_TOKEN,
    RES_HOLD_EXPIRED,
    RES_DB_ERROR,
    RES_INTERNAL_ERR
} res_code_t;

// Lightweight seat view returned to callers (safe, read-only fields)
typedef struct {
    char event_id[RES_ID_LEN];
    char seat_id[RES_ID_LEN];
    tb_money_cents_t price_cents;
    seat_status_t status;

    // present when status == SEAT_HELD
    char  holder_user_id[RES_ID_LEN];
    tb_epoch_t hold_expires_unix;
} seat_view_t;

// Result of placing a hold
typedef struct {
    res_code_t code;
    tb_money_cents_t price_cents;
    tb_epoch_t expires_unix;
    tb_byte_t hold_token[RES_TOKEN_LEN];
    size_t token_len;
} hold_result_t;

// Result of confirming a reservation (purchase)
typedef struct {
    res_code_t code;
    char order_id[RES_ID_LEN];
    tb_money_cents_t price_cents;
} confirm_result_t;

// Lifecycle
bool reservation_init(void);
void reservation_shutdown(void);

// Test/utility helpers
// Insert or replace a seat in the in-memory map (used by tests/seed data).
bool reservation_put_seat(const seat_t *seat);

// Adjust the default hold length (seconds). Useful for tests.
void reservation_set_hold_length_seconds(tb_epoch_t seconds);

// Core operations
hold_result_t place_hold(const char *user_id,
                         const char *event_id,
                         const char *seat_id);

confirm_result_t confirm_reservation(const tb_byte_t *hold_token,
                                     size_t token_len,
                                     tb_money_cents_t amount_paid_cents);

res_code_t cancel_hold(const char *user_id,
                       const char *event_id,
                       const char *seat_id);

bool seat_get(const char *event_id,
              const char *seat_id,
              seat_view_t *out);

res_code_t refund(const char *user_id,
                  const char *order_id);

#ifdef __cplusplus
}
#endif
