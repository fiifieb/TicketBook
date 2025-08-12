#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hashtable.h"
#include "types.h"

static uint64_t hash_key(const char *event_id, const char *seat_id)
{
    uint64_t hash = 1469598103934665603ULL; // FNV-1a offset
    const char *parts[2] = {event_id, seat_id};

    for (int i = 0; i < 2; i++)
    {
        const char *p = parts[i];
        while (*p)
        {
            hash ^= (unsigned char)*p++;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
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