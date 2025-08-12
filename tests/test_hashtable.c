// gcc -Wall -Wextra -Iinclude -L. tests/test_hashtable.c src/hashtable.c src/utils.c -lpthread -o test_hashtable
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "hashtable.h"
#include "types.h"

static seat_t mkseat(const char *ev, const char *seat, int price)
{
    seat_t s = {0};
    strncpy(s.event_id, ev, sizeof(s.event_id) - 1);
    strncpy(s.seat_id, seat, sizeof(s.seat_id) - 1);
    s.price_cents = price;
    s.status = SEAT_AVAILABLE;
    return s;
}

static void test_create_put_get(void)
{
    seat_map_t *m = seat_map_create(128);
    assert(m);

    seat_t a = mkseat("E1", "A1", 1000);
    seat_t b = mkseat("E1", "A2", 1200);
    seat_t c = mkseat("E2", "X9", 9900);

    assert(seat_map_put(m, &a));
    assert(seat_map_put(m, &b));
    assert(seat_map_put(m, &c));

    seat_t out = {0};
    assert(seat_map_get(m, "E1", "A1", &out));
    assert(out.price_cents == 1000);

    assert(seat_map_get(m, "E1", "A2", &out));
    assert(out.price_cents == 1200);

    assert(seat_map_get(m, "E2", "X9", &out));
    assert(out.price_cents == 9900);

    seat_map_destroy(m);
    printf("[OK] create/put/get\n");
}

static void test_update_in_place(void)
{
    seat_map_t *m = seat_map_create(64);
    seat_t a = mkseat("E1", "A1", 1000);
    assert(seat_map_put(m, &a));

    // Update price
    a.price_cents = 1500;
    assert(seat_map_put(m, &a));

    seat_t out = {0};
    assert(seat_map_get(m, "E1", "A1", &out));
    assert(out.price_cents == 1500);

    seat_map_destroy(m);
    printf("[OK] update in place\n");
}

static void test_lock_unlock(void)
{
    seat_map_t *m = seat_map_create(64);
    seat_t a = mkseat("E1", "A1", 1000);
    assert(seat_map_put(m, &a));

    // lock
    assert(seat_map_lock(m, "E1", "A1"));
    // while locked, safe_get should block or succeed after we unlock
    seat_map_unlock(m, "E1", "A1");

    seat_map_destroy(m);
    printf("[OK] lock/unlock\n");
}

typedef struct
{
    seat_map_t *m;
    const char *ev;
    const char *sid;
    int loops;
} worker_arg;

static void *worker_fn(void *p)
{
    worker_arg *w = (worker_arg *)p;
    for (int i = 0; i < w->loops; ++i)
    {
        // lock, read, mutate price, write back
        if (seat_map_lock(w->m, w->ev, w->sid))
        {
            seat_t s = {0};
            if (seat_map_get(w->m, w->ev, w->sid, &s))
            {
                s.price_cents += 1;
                (void)seat_map_put(w->m, &s);
            }
            (void)seat_map_unlock(w->m, w->ev, w->sid);
        }
    }
    return NULL;
}

static void test_concurrent_rw(void)
{
    seat_map_t *m = seat_map_create(256);
    seat_t s = mkseat("E1", "A1", 0);
    assert(seat_map_put(m, &s));

    worker_arg arg = {.m = m, .ev = "E1", .sid = "A1", .loops = 10000};

    pthread_t t1, t2, t3, t4;
    pthread_create(&t1, NULL, worker_fn, &arg);
    pthread_create(&t2, NULL, worker_fn, &arg);
    pthread_create(&t3, NULL, worker_fn, &arg);
    pthread_create(&t4, NULL, worker_fn, &arg);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);

    seat_t out = {0};
    assert(seat_map_get(m, "E1", "A1", &out));
    // Expected final value = sum of increments from 4 threads
    assert(out.price_cents == 4 * arg.loops);

    seat_map_destroy(m);
    printf("[OK] concurrent read/modify/write\n");
}

int main(void)
{
    test_create_put_get();
    test_update_in_place();
    test_lock_unlock();
    test_concurrent_rw();
    printf("All hashtable tests passed.\n");
    return 0;
}
