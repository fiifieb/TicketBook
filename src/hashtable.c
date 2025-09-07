#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hashtable.h"
#include "types.h"
#include "utils.h"

static inline uint64_t hash_key(const char *event_id, const char *seat_id)
{
    return tb_hash_key_fast(event_id, seat_id);
}

void destroy_bucket(bucket_t *bucket)
{
    pthread_mutex_destroy(&bucket->mtx);
    free(bucket);
}
/* ---- Lifecycle ---- */

seat_map_t *seat_map_create(size_t capacity)
{
    seat_map_t *map = malloc(sizeof(seat_map_t));
    map->cap = capacity;
    map->table = calloc(capacity, sizeof(bucket_t *));
    return map;
}

void seat_map_destroy(seat_map_t *m)
{
    if (!m)
        return;

    for (size_t i = 0; i < m->cap; ++i)
    {
        bucket_t *curr = m->table[i];
        while (curr)
        {
            bucket_t *next = curr->next;
            pthread_mutex_destroy(&curr->mtx);
            free(curr);
            curr = next;
        }
    }

    free(m->table);
    free(m);
}

bool seat_map_put(seat_map_t *m, const seat_t *seat)
{
    if (!m || !seat)
        return false;
    size_t idx = hash_key(seat->event_id, seat->seat_id) % m->cap;
    bucket_t *curr = m->table[idx];
    while (curr)
    {
        if (strcmp(curr->seat.event_id, seat->event_id) == 0 &&
            strcmp(curr->seat.seat_id, seat->seat_id) == 0)
        {
            curr->seat = *seat;
            return true;
        }
        curr = curr->next;
    }

    bucket_t *node = malloc(sizeof(*node));
    if (!node)
        return false;
    node->seat = *seat;
    pthread_mutex_init(&node->mtx, NULL);
    node->next = m->table[idx];
    m->table[idx] = node;

    return true;
}

bool seat_map_delete(seat_map_t *m,
                     const char *event_id,
                     const char *seat_id)
{
    if (!m || !event_id || !seat_id)
        return false;
    size_t idx = hash_key(event_id, seat_id) % m->cap;
    bucket_t *curr = m->table[idx];
    bucket_t *prev = NULL;
    while (curr)
    {
        if (strcmp(curr->seat.event_id, event_id) == 0 &&
            strcmp(curr->seat.seat_id, seat_id) == 0)
        {
            if (prev)
            {
                prev->next = curr->next;
            }
            else
            {
                m->table[idx] = curr->next;
            }
            destroy_bucket(curr);
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

bool seat_map_get(seat_map_t *m,
                  const char *event_id,
                  const char *seat_id,
                  seat_t *out)
{
    if (!m || !event_id || !seat_id)
        return false;
    size_t idx = hash_key(event_id, seat_id) % m->cap;
    bucket_t *curr = m->table[idx];
    while (curr)
    {
        if (strcmp(curr->seat.event_id, event_id) == 0 &&
            strcmp(curr->seat.seat_id, seat_id) == 0)
        {
            *out = curr->seat;
            return true;
        }
        curr = curr->next;
    }
    return false;
}

/* ---- Concurrency helpers ---- */

bool seat_map_lock(seat_map_t *m,
                   const char *event_id,
                   const char *seat_id)
{
    if (!m || !event_id || !seat_id)
        return false;
    size_t idx = hash_key(event_id, seat_id) % m->cap;
    bucket_t *curr = m->table[idx];
    while (curr)
    {
        if (strcmp(curr->seat.event_id, event_id) == 0 &&
            strcmp(curr->seat.seat_id, seat_id) == 0)
        {
            return pthread_mutex_lock(&(curr->mtx)) == 0;
        }
        curr = curr->next;
    }
    return false;
}

void seat_map_unlock(seat_map_t *m,
                     const char *event_id,
                     const char *seat_id)
{
    if (!m || !event_id || !seat_id)
        return;
    size_t idx = hash_key(event_id, seat_id) % m->cap;
    bucket_t *curr = m->table[idx];
    while (curr)
    {
        if (strcmp(curr->seat.event_id, event_id) == 0 &&
            strcmp(curr->seat.seat_id, seat_id) == 0)
        {
            pthread_mutex_unlock(&(curr->mtx));
            return;
        }
        curr = curr->next;
    }
}

bool seat_map_find_by_token(seat_map_t *m,
                            const tb_byte_t *token,
                            size_t token_len,
                            seat_t *out)
{
    if (!m || !token || token_len == 0 || !out)
        return false;

    for (size_t i = 0; i < m->cap; ++i)
    {
        bucket_t *curr = m->table[i];
        while (curr)
        {
            if (curr->seat.status == SEAT_HELD &&
                curr->seat.hold_token_len == token_len &&
                tb_memcmp_token32(curr->seat.hold_token, token, token_len) == 0)
            {
                *out = curr->seat;
                return true;
            }
            curr = curr->next;
        }
    }
    return false;
}
