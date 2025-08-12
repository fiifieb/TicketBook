#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

#include "types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct bucket bucket_t; // forward declare
    typedef struct seat_map seat_map_t;

    struct bucket
    {
        seat_t seat;
        pthread_mutex_t mtx;
        bucket_t *next;
    };

    struct seat_map
    {
        size_t cap; // number of buckets
        bucket_t **table;
    };

    // Create a new seat map with given capacity (rounded up internally).
    // Returns NULL on allocation failure.
    seat_map_t *seat_map_create(size_t capacity);

    // Free all buckets and associated locks.
    void seat_map_destroy(seat_map_t *m);

    // Insert or replace a seat entry.
    // Returns true on success.
    bool seat_map_put(seat_map_t *m, const seat_t *seat);

    // Remove a seat entry entirely (if it exists).
    // Returns true if removed, false if not found.
    bool seat_map_delete(seat_map_t *m,
                         const char *event_id,
                         const char *seat_id);

    // Retrieve a seat by event + seat id.
    // Returns true and copies into *out if found.
    bool seat_map_get(seat_map_t *m,
                      const char *event_id,
                      const char *seat_id,
                      seat_t *out);

    // ---- Concurrency helpers ----

    // Lock the mutex for a specific seat if it exists.
    // Call before mutating the seat struct in place.
    // Returns true if locked.
    bool seat_map_lock(seat_map_t *m,
                       const char *event_id,
                       const char *seat_id);

    // Unlock the mutex for a specific seat.
    // Call after mutating seat struct.
    void seat_map_unlock(seat_map_t *m,
                         const char *event_id,
                         const char *seat_id);

#ifdef __cplusplus
}
#endif