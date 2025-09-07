// Unit tests for DB stub interface
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "db_interface.h"

int main(void)
{
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

    // Refund
    db_txn_t *t3 = db_txn_begin();
    assert(db_refund_create(t3, "U1", order_id, 1234) == RES_OK);
    assert(db_txn_commit(t3));

    printf("All DB interface tests passed.\n");
    return 0;
}

