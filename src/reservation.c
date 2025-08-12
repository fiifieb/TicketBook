#include "reservation.h"
#include "hashtable.h"
#include "types.h"
#include "config.h"

// ---- internal state ----
static seat_map_t* g_map = NULL;

// ---- helpers ----
static long now_unix(void);
static void random_bytes(unsigned char* out, size_t n);
static void clear_hold_fields(seat_view_t* s);
static void expire_if_needed(seat_view_t* s);
static void to_view(const seat_t* in, seat_view_t* out);

// ---- API implementation ----

bool reservation_init(void) {
    // TODO: allocate seat_map and initialize state
    return false;
}

void reservation_shutdown(void) {
    // TODO: free resources
}

hold_result_t place_hold(const char* user_id,
                         const char* event_id,
                         const char* seat_id) {
    hold_result_t res = {0};
    // TODO: implement seat hold logic
    return res;
}

confirm_result_t confirm_reservation(const unsigned char* hold_token,
                                     size_t token_len,
                                     int amount_paid_cents) {
    confirm_result_t out = {0};
    // TODO: implement confirm logic
    return out;
}

res_code_t cancel_hold(const char* user_id,
                       const char* event_id,
                       const char* seat_id) {
    // TODO: implement cancel hold logic
    return RES_INTERNAL_ERR;
}

bool seat_get(const char* event_id,
              const char* seat_id,
              seat_view_t* out) {
    // TODO: implement seat_get logic
    return false;
}

res_code_t refund(const char* user_id,
                  const char* order_id) {
    // TODO: implement refund logic
    return RES_INTERNAL_ERR;
}