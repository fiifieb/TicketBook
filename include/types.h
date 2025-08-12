#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- fixed sizes for IDs/tokens (tweak if you like)
#define TB_ID_LEN       32    // user_id, event_id, seat_id, order_id (ascii, nul-terminated)
#define TB_TOKEN_LEN    32    // raw bytes for hold token (not hex)

// ---- seat status (matches reservation.h semantics)
typedef enum {
    SEAT_AVAILABLE = 0,
    SEAT_HELD      = 1,
    SEAT_SOLD      = 2,
    SEAT_REFUNDED  = 3   // optional; distinct from AVAILABLE for auditing
} seat_status_t;

// ---- core seat record stored in the concurrent hashtable
typedef struct {
    // identity
    char event_id[TB_ID_LEN];      // e.g., "E123"
    char seat_id[TB_ID_LEN];       // e.g., "A12"

    // pricing (authoritative price always comes from DB at confirm time;
    // keep here for fast reads/UI hints)
    int  price_cents;

    // state machine
    seat_status_t status;          // AVAILABLE/HELD/SOLD/REFUNDED

    // hold info (valid when status == HELD)
    char  holder_user_id[TB_ID_LEN];    // who currently holds it
    long  hold_expires_unix;            // epoch seconds; 0 if no hold
    uint8_t hold_token[TB_TOKEN_LEN];   // opaque random token
    size_t  hold_token_len;             // actual bytes used (<= TB_TOKEN_LEN)

    // sale info (valid when status == SOLD or REFUNDED)
    char last_order_id[TB_ID_LEN];      // set on successful confirm; stable id

    // bookkeeping
    uint32_t version;               // monotonic counter for optimistic checks (optional)
    long updated_unix;              // last in-memory update time (optional)
} seat_t;

#ifdef __cplusplus
}
#endif