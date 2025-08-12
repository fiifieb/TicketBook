#pragma once
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- Constants ----
#define RES_TOKEN_LEN 32      // bytes (not hex-encoded)
#define RES_ID_LEN    32      // for user_id, event_id, seat_id, order_id

// ---- Seat status (in-memory mirror) ----
typedef enum {
    SEAT_AVAILABLE = 0,
    SEAT_HELD      = 1,
    SEAT_SOLD      = 2
} seat_status_t;

// ---- Result codes for reservation ops ----
typedef enum {
    RES_OK = 0,
    RES_NOT_FOUND,
    RES_ALREADY_SOLD,
    RES_HELD_BY_OTHER,
    RES_HOLD_EXISTS_SAME_USER,
    RES_HOLD_EXPIRED,
    RES_INVALID_TOKEN,
    RES_DB_ERROR,
    RES_INTERNAL_ERR
} res_code_t;

// ---- Public seat view ----
typedef struct {
    char event_id[RES_ID_LEN];
    char seat_id[RES_ID_LEN];
    int  price_cents;
    seat_status_t status;
    char holder_user_id[RES_ID_LEN]; // empty if none
    long hold_expires_unix;          // 0 if none
} seat_view_t;

// ---- place_hold outputs a token + expiry + price ----
typedef struct {
    res_code_t code;
    unsigned char hold_token[RES_TOKEN_LEN]; // opaque binary; return length in token_len
    size_t token_len;
    long   expires_unix;                      // epoch seconds
    int    price_cents;                       // authoritative price
} hold_result_t;

// ---- confirm_reservation returns order info ----
typedef struct {
    res_code_t code;
    char order_id[RES_ID_LEN]; // empty on failure
    int  price_cents;          // charged price
} confirm_result_t;

// ---- Lifecycle ----
// Initialize in-memory structures (hash table, timers, etc.).
// Returns false on allocation/init failure.
bool reservation_init(void);

// Free resources.
void reservation_shutdown(void);

// ---- Core operations ----

// Attempt to place a hold for (user_id, event_id, seat_id).
// Generates a hold token and sets expiry (~TTL).
// Thread-safe: acquires per-seat lock internally.
hold_result_t place_hold(const char* user_id,
                         const char* event_id,
                         const char* seat_id);

// Confirm a held seat using the hold token (binary bytes).
// amount_paid_cents is validated against authoritative price.
// Performs DB transaction to mark SOLD and create order.
// Thread-safe; idempotent per token if you implement an idempotency table.
confirm_result_t confirm_reservation(const unsigned char* hold_token,
                                     size_t token_len,
                                     int amount_paid_cents);

// Cancel a hold explicitly (optional; holds also expire).
res_code_t cancel_hold(const char* user_id,
                       const char* event_id,
                       const char* seat_id);

// Retrieve current seat view (after lazy-expiring holds if needed).
bool seat_get(const char* event_id,
              const char* seat_id,
              seat_view_t* out);

// Refund a previously sold order (policy-dependent).
// You can later choose whether SOLD -> AVAILABLE or -> REFUNDED state.
res_code_t refund(const char* user_id,
                  const char* order_id);

#ifdef __cplusplus
}
#endif