#include "reservation.h"
#include "hashtable.h"
#include "types.h"
#include "config.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "db_interface.h"
#include "utils.h"

#ifndef CONFIG_SEATMAP_INITIAL_CAPACITY
#define CONFIG_SEATMAP_INITIAL_CAPACITY 16384u
#endif

// Default hold length (seconds). Can be adjusted by configuration.
static tb_epoch_t g_hold_length_secs = 300; // 5 minutes

static pthread_once_t g_reservation_once = PTHREAD_ONCE_INIT;
static bool g_reservation_init_ok = false;

// ---- internal state ----
static seat_map_t *g_map = NULL;

// Lookup a seat by its hold token. Returns true and fills *out on success.
// This function should NOT lock; we will lock by (event_id, seat_id) once resolved.
extern bool seat_map_find_by_token(seat_map_t *map,
                                   const tb_byte_t *token,
                                   size_t token_len,
                                   seat_t *out);

static void reservation_do_init(void)
{
    // Guard against double-init in case this is ever called directly.
    if (g_map != NULL)
    {
        g_reservation_init_ok = true;
        return;
    }

    // Create the seat map with a default capacity; override via config.h if desired.
    g_map = seat_map_create(CONFIG_SEATMAP_INITIAL_CAPACITY);
    if (g_map == NULL)
    {
        g_reservation_init_ok = false;
        return;
    }

    g_reservation_init_ok = true;
}

// ---- helpers ----
static long now_unix(void)
{
    return (long)time(NULL);
}

// use utils helper for random bytes (keeps platform-specific code in one place)
static inline void random_bytes(unsigned char *out, size_t n)
{ tb_random_bytes_fast(out, n); }

static void clear_hold_fields(seat_t *s)
{
    if (!s)
        return;
    memset(s->holder_user_id, 0, sizeof s->holder_user_id);
    memset(s->hold_token, 0, sizeof s->hold_token);
    s->hold_token_len = 0;
    s->hold_expires_unix = 0;
}

static void to_view(const seat_t *in, seat_view_t *out)
{
    if (!in || !out)
        return;
    memset(out, 0, sizeof(*out));
    strncpy(out->event_id, in->event_id, RES_ID_LEN - 1);
    strncpy(out->seat_id, in->seat_id, RES_ID_LEN - 1);
    out->price_cents = in->price_cents;
    out->status = in->status;
    if (in->status == SEAT_HELD)
    {
        strncpy(out->holder_user_id, in->holder_user_id, RES_ID_LEN - 1);
        out->hold_expires_unix = in->hold_expires_unix;
    }
}

// ---- API implementation ----

bool reservation_init(void)
{
    // Ensure thread-safe one-time initialization even if multiple threads call this concurrently.
    pthread_once(&g_reservation_once, reservation_do_init);
    return g_reservation_init_ok;
}

void reservation_shutdown(void)
{
    if (g_map)
    {
        seat_map_destroy(g_map);
        g_map = NULL;
    }
    // Allow init to run again for different seats.
    g_reservation_once = (pthread_once_t)PTHREAD_ONCE_INIT;
    g_reservation_init_ok = false;
}

bool reservation_put_seat(const seat_t *seat)
{
    if (!g_reservation_init_ok || !g_map || !seat)
        return false;
    return seat_map_put(g_map, seat);
}

void reservation_set_hold_length_seconds(tb_epoch_t seconds)
{
    if (seconds < 0) seconds = 0;
    g_hold_length_secs = seconds;
}

hold_result_t place_hold(const char *user_id,
                         const char *event_id,
                         const char *seat_id)
{
    hold_result_t res;
    memset(&res, 0, sizeof(res));

    if (!user_id || !event_id || !seat_id)
    {
        res.code = RES_NOT_FOUND; // treat invalid/NULL ids as not found
        return res;
    }

    // Try to acquire the per-seat lock
    if (!seat_map_lock(g_map, event_id, seat_id))
    {
        res.code = RES_HELD_BY_OTHER;
        return res;
    }

    // Fetch the seat from the map
    seat_t s = {0};
    bool ok = seat_map_get(g_map, event_id, seat_id, &s);
    if (!ok)
    {
        // seat not found
        seat_map_unlock(g_map, event_id, seat_id);
        res.code = RES_NOT_FOUND;
        return res;
    }

    // Respect current state before taking the seat
    tb_epoch_t now = now_unix();
    if (s.status == SEAT_SOLD)
    {
        seat_map_unlock(g_map, event_id, seat_id);
        res.code = RES_ALREADY_SOLD;
        return res;
    }
    if (s.status == SEAT_HELD)
    {
        const bool same_user = (strncmp(s.holder_user_id, user_id, RES_ID_LEN) == 0);
        const bool expired = (s.hold_expires_unix > 0 && now >= s.hold_expires_unix);
        if (!expired)
        {
            if (same_user)
            {
                // Existing active hold by same user → return existing details
                seat_map_unlock(g_map, event_id, seat_id);
                res.code = RES_HOLD_EXISTS_SAME_USER;
                res.price_cents = s.price_cents;
                res.expires_unix = s.hold_expires_unix;
                res.token_len = s.hold_token_len;
                memcpy(res.hold_token, s.hold_token, s.hold_token_len);
                return res;
            }
            else
            {
                seat_map_unlock(g_map, event_id, seat_id);
                res.code = RES_HELD_BY_OTHER;
                return res;
            }
        }
        // if expired, fall through to create a fresh hold
    }

    // Create/refresh hold for this user
    s.status = SEAT_HELD;
    memset(s.holder_user_id, 0, sizeof s.holder_user_id);
    strncpy(s.holder_user_id, user_id, RES_ID_LEN - 1);
    s.hold_expires_unix = now + g_hold_length_secs;
    s.hold_token_len = RES_TOKEN_LEN;
    random_bytes(s.hold_token, s.hold_token_len);

    // Persist back into the map
    seat_map_put(g_map, &s);

    // Release the lock before returning
    seat_map_unlock(g_map, event_id, seat_id);

    // Fill minimal result: success code and authoritative price for the UI
    res.code = RES_OK;
    res.price_cents = s.price_cents;
    res.expires_unix = s.hold_expires_unix;
    res.token_len = s.hold_token_len;
    memcpy(res.hold_token, s.hold_token, s.hold_token_len);

    return res;
}

confirm_result_t confirm_reservation(const tb_byte_t *hold_token,
                                     size_t token_len,
                                     tb_money_cents_t amount_paid_cents)
{
    confirm_result_t out;
    memset(&out, 0, sizeof(out));

    // 1) Validate token
    if (!hold_token || token_len == 0 || token_len > RES_TOKEN_LEN)
    {
        out.code = RES_INVALID_TOKEN;
        return out;
    }

    // 2) Idempotency: if an order already exists for this token, return it
    tb_money_cents_t prev_price = 0;
    char existing_order_id[RES_ID_LEN] = {0};
    res_code_t rc = db_order_find_by_token(hold_token, token_len,
                                           existing_order_id, &prev_price);
    if (rc == RES_OK)
    {
        out.code = RES_OK;
        strncpy(out.order_id, existing_order_id, RES_ID_LEN - 1);
        out.price_cents = prev_price;
        return out;
    }
    else if (rc == RES_DB_ERROR)
    {
        out.code = RES_DB_ERROR;
        return out;
    }

    // 3) Resolve which seat this token belongs to (no lock yet)
    seat_t s = {0};
    if (!seat_map_find_by_token(g_map, hold_token, token_len, &s))
    {
        out.code = RES_INVALID_TOKEN; // unknown token
        return out;
    }

    // 4) Lock the concrete seat and re-read to avoid TOCTOU
    if (!seat_map_lock(g_map, s.event_id, s.seat_id))
    {
        out.code = RES_HELD_BY_OTHER; // someone else operating on this seat
        return out;
    }

    bool ok = seat_map_get(g_map, s.event_id, s.seat_id, &s);
    if (!ok)
    {
        seat_map_unlock(g_map, s.event_id, s.seat_id);
        out.code = RES_NOT_FOUND;
        return out;
    }

    // 5) Validate held state, token, and expiry
    if (s.status != SEAT_HELD || s.hold_token_len != token_len ||
        memcmp(s.hold_token, hold_token, token_len) != 0)
    {
        seat_map_unlock(g_map, s.event_id, s.seat_id);
        out.code = RES_INVALID_TOKEN;
        return out;
    }

    tb_epoch_t now = now_unix();
    if (s.hold_expires_unix > 0 && now >= s.hold_expires_unix)
    {
        // expire and persist
        s.status = SEAT_AVAILABLE;
        clear_hold_fields(&s);
        seat_map_put(g_map, &s);
        seat_map_unlock(g_map, s.event_id, s.seat_id);
        out.code = RES_HOLD_EXPIRED;
        return out;
    }

    // 6) Determine authoritative price (DB may override in-memory)
    tb_money_cents_t price = s.price_cents;
    tb_money_cents_t db_price = 0;
    rc = db_authoritative_price(s.event_id, s.seat_id, &db_price);
    if (rc == RES_OK && db_price > 0)
        price = db_price;
    else if (rc == RES_DB_ERROR)
    {
        seat_map_unlock(g_map, s.event_id, s.seat_id);
        out.code = RES_DB_ERROR;
        return out;
    }

    // 6.5) Enforce caller-paid amount equals authoritative price
    if (amount_paid_cents != price)
    {
        seat_map_unlock(g_map, s.event_id, s.seat_id);
        out.code = RES_INTERNAL_ERR; // payment amount mismatch
        return out;
    }

    // 7) Create order and mark seat SOLD in a single DB transaction
    db_txn_t *txn = db_txn_begin();
    if (!txn)
    {
        seat_map_unlock(g_map, s.event_id, s.seat_id);
        out.code = RES_DB_ERROR;
        return out;
    }

    char order_id[RES_ID_LEN] = {0};
    rc = db_order_create(txn, s.holder_user_id, s.event_id, s.seat_id,
                         price, hold_token, token_len, order_id);
    if (rc == RES_OK)
    {
        rc = db_seat_mark_sold(txn, s.event_id, s.seat_id, order_id);
    }

    if (rc != RES_OK || !db_txn_commit(txn))
    {
        db_txn_rollback(txn);
        seat_map_unlock(g_map, s.event_id, s.seat_id);
        out.code = (rc == RES_DB_ERROR ? RES_DB_ERROR : RES_INTERNAL_ERR);
        return out;
    }

    // 8) Update in-memory seat to SOLD and clear hold
    s.status = SEAT_SOLD;
    clear_hold_fields(&s);
    seat_map_put(g_map, &s);
    seat_map_unlock(g_map, s.event_id, s.seat_id);

    // 9) Return success
    out.code = RES_OK;
    strncpy(out.order_id, order_id, RES_ID_LEN - 1);
    out.price_cents = price;
    return out;
}

res_code_t cancel_hold(const char *user_id,
                       const char *event_id,
                       const char *seat_id)
{
    if (!user_id || !event_id || !seat_id)
    {
        return RES_NOT_FOUND; // invalid identifiers treated as not found
    }

    if (!seat_map_lock(g_map, event_id, seat_id))
    {
        return RES_NOT_FOUND; // seat missing or lock failed
    }

    seat_t s = {0};
    bool ok = seat_map_get(g_map, event_id, seat_id, &s);
    if (!ok)
    {
        seat_map_unlock(g_map, event_id, seat_id);
        return RES_NOT_FOUND;
    }

    // Only the current holder can cancel; and there must be an active hold
    if (s.status != SEAT_HELD)
    {
        seat_map_unlock(g_map, event_id, seat_id);
        return (s.status == SEAT_SOLD) ? RES_ALREADY_SOLD : RES_NOT_FOUND;
    }
    if (strncmp(s.holder_user_id, user_id, RES_ID_LEN) != 0)
    {
        seat_map_unlock(g_map, event_id, seat_id);
        return RES_HELD_BY_OTHER;
    }

    // Cancel the hold → AVAILABLE
    s.status = SEAT_AVAILABLE;
    clear_hold_fields(&s);
    seat_map_put(g_map, &s);
    seat_map_unlock(g_map, event_id, seat_id);
    return RES_OK;
}

bool seat_get(const char *event_id,
              const char *seat_id,
              seat_view_t *out)
{
    if (!event_id || !seat_id || !out)
        return false;

    if (!seat_map_lock(g_map, event_id, seat_id))
    {
        return false; // seat not found or failed to lock
    }

    // Read the current seat (internal model), expire hold if needed, then copy a view.
    seat_t internal = {0};
    bool ok = seat_map_get(g_map, event_id, seat_id, &internal);
    if (ok)
    {
        // Lazy-expire inside the lock to keep state consistent.
        if (internal.status == SEAT_HELD && internal.hold_expires_unix > 0)
        {
            tb_epoch_t now = (tb_epoch_t)now_unix();
            if (now >= internal.hold_expires_unix)
            {
                // Clear hold fields and flip to AVAILABLE.
                internal.status = SEAT_AVAILABLE;
                clear_hold_fields(&internal);
                // Persist the change back to the map.
                seat_map_put(g_map, &internal);
            }
        }

        // Convert to public view for callers.
        to_view(&internal, out);
    }
    seat_map_unlock(g_map, event_id, seat_id);
    return ok;
}

res_code_t refund(const char *user_id,
                  const char *order_id)
{
    if (!user_id || !order_id)
    {
        return RES_NOT_FOUND;
    }

    // 1) Look up order details by id
    char ord_user[RES_ID_LEN] = {0};
    char ev_id[RES_ID_LEN] = {0};
    char st_id[RES_ID_LEN] = {0};
    tb_money_cents_t price = 0;
    res_code_t rc = db_order_find_by_id(order_id, ord_user, ev_id, st_id, &price);
    if (rc == RES_NOT_FOUND)
        return RES_NOT_FOUND;
    if (rc == RES_DB_ERROR)
        return RES_DB_ERROR;

    // Optional: enforce the requester matches the order's user
    if (strncmp(ord_user, user_id, RES_ID_LEN) != 0)
    {
        // For security, do not leak order existence; treat as not found
        return RES_NOT_FOUND;
    }

    // 2) Begin refund txn
    db_txn_t *txn = db_txn_begin();
    if (!txn)
        return RES_DB_ERROR;

    rc = db_refund_create(txn, user_id, order_id, price);
    if (rc != RES_OK || !db_txn_commit(txn))
    {
        db_txn_rollback(txn);
        return (rc == RES_DB_ERROR ? RES_DB_ERROR : RES_INTERNAL_ERR);
    }

    // 3) Flip in-memory seat state from SOLD → AVAILABLE (best-effort)
    if (seat_map_lock(g_map, ev_id, st_id))
    {
        seat_t s = {0};
        if (seat_map_get(g_map, ev_id, st_id, &s))
        {
            if (s.status == SEAT_SOLD)
            {
                s.status = SEAT_AVAILABLE; // or SEAT_REFUNDED if your enum supports it
                clear_hold_fields(&s);
                seat_map_put(g_map, &s);
            }
        }
        seat_map_unlock(g_map, ev_id, st_id);
    }

    return RES_OK;
}
