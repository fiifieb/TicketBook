// Utility helpers with RISC-V optimized paths and portable fallbacks
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Compare up to 32 bytes quickly; returns 0 if equal, non-zero otherwise.
int tb_memcmp_token32(const void *a, const void *b, size_t n);

// Fast 64-bit hash for (event_id, seat_id) pair.
uint64_t tb_hash_key_fast(const char *event_id, const char *seat_id);

// Fill buffer with random bytes using best available source on this platform.
void tb_random_bytes_fast(unsigned char *out, size_t n);

#ifdef __cplusplus
}
#endif
