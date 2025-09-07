// Unit tests for reservation API
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "reservation.h"
#include "types.h"

static seat_t mkseat(const char *ev, const char *sid, tb_money_cents_t price)
{
    seat_t s = (seat_t){0};
    strncpy(s.event_id, ev, TB_ID_LEN - 1);
    strncpy(s.seat_id, sid, TB_ID_LEN - 1);
    s.price_cents = price;
    s.status = SEAT_AVAILABLE;
    return s;
}

static void test_hold_confirm_cancel_flow(void)
{
    assert(reservation_init());
    // seed seat
    seat_t s = mkseat("EV1", "S01", 2500);
    assert(reservation_put_seat(&s));

    // seat_get available
    seat_view_t v = {0};
    assert(seat_get("EV1", "S01", &v));
    assert(v.status == SEAT_AVAILABLE);

    // place_hold
    hold_result_t h = place_hold("U1", "EV1", "S01");
    assert(h.code == RES_OK);
    assert(h.token_len > 0);
    tb_byte_t token[RES_TOKEN_LEN];
    size_t token_len = h.token_len;
    memcpy(token, h.hold_token, token_len);

    // seat_get shows held by U1
    assert(seat_get("EV1", "S01", &v));
    assert(v.status == SEAT_HELD);
    assert(strncmp(v.holder_user_id, "U1", RES_ID_LEN) == 0);

    // repeat hold same user returns existing hold info
    hold_result_t h2 = place_hold("U1", "EV1", "S01");
    assert(h2.code == RES_HOLD_EXISTS_SAME_USER);
    assert(h2.token_len == token_len);
    assert(memcmp(h2.hold_token, token, token_len) == 0);

    // other user cannot hold
    hold_result_t h3 = place_hold("U2", "EV1", "S01");
    assert(h3.code == RES_HELD_BY_OTHER);

    // confirm with wrong amount → error
    confirm_result_t c0 = confirm_reservation(token, token_len, 999);
    assert(c0.code == RES_INTERNAL_ERR);

    // confirm with correct amount
    confirm_result_t c1 = confirm_reservation(token, token_len, 2500);
    assert(c1.code == RES_OK);
    assert(strlen(c1.order_id) > 0);
    assert(c1.price_cents == 2500);

    // seat becomes sold
    assert(seat_get("EV1", "S01", &v));
    assert(v.status == SEAT_SOLD);

    // cancel_hold on sold seat → already sold
    assert(cancel_hold("U1", "EV1", "S01") == RES_ALREADY_SOLD);

    // refund wrong user → not found
    assert(refund("U2", c1.order_id) == RES_NOT_FOUND);

    // refund correct user
    assert(refund("U1", c1.order_id) == RES_OK);

    // seat available again
    assert(seat_get("EV1", "S01", &v));
    assert(v.status == SEAT_AVAILABLE);

    reservation_shutdown();
    printf("[OK] hold/confirm/cancel/refund flow\n");
}

static void test_cancel_hold_and_expiry(void)
{
    assert(reservation_init());
    reservation_set_hold_length_seconds(1); // make holds expire quickly

    seat_t s = mkseat("EV2", "S02", 1000);
    assert(reservation_put_seat(&s));

    hold_result_t h = place_hold("U9", "EV2", "S02");
    assert(h.code == RES_OK);

    // cancel hold explicitly
    assert(cancel_hold("U9", "EV2", "S02") == RES_OK);
    seat_view_t v = {0};
    assert(seat_get("EV2", "S02", &v) && v.status == SEAT_AVAILABLE);

    // place hold again to test expiry
    h = place_hold("U9", "EV2", "S02");
    assert(h.code == RES_OK);
    sleep(2); // allow expiry
    assert(seat_get("EV2", "S02", &v));
    assert(v.status == SEAT_AVAILABLE); // lazy expiry on get

    reservation_shutdown();
    printf("[OK] cancel hold and expiry\n");
}

int main(void)
{
    test_hold_confirm_cancel_flow();
    test_cancel_hold_and_expiry();
    printf("All reservation tests passed.\n");
    return 0;
}

