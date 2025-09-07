// Portable implementations with RISC-V aware fast paths (no lock changes)
#include "utils.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  #include <stdlib.h>
#endif

static inline uint64_t splitmix64(uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

int tb_memcmp_token32(const void *a, const void *b, size_t n)
{
    if (n == 0) return 0;
    if (n > 32) n = 32; // cap

#if defined(__riscv) && __riscv_xlen == 64
    // 64-bit word-wise compare via memcpy to avoid alignment UB
    const unsigned char *ca = (const unsigned char *)a;
    const unsigned char *cb = (const unsigned char *)b;
    while (n >= 8) {
        uint64_t wa = 0, wb = 0;
        memcpy(&wa, ca, 8);
        memcpy(&wb, cb, 8);
        if ((wa ^ wb) != 0) return 1;
        ca += 8; cb += 8; n -= 8;
    }
    while (n--) {
        if (*ca++ != *cb++) return 1;
    }
    return 0;
#else
    return memcmp(a, b, n);
#endif
}

uint64_t tb_hash_key_fast(const char *event_id, const char *seat_id)
{
    // Cheap but solid: hash the two strings with splitmix64 over bytes
    uint64_t h1 = 0x1234567890abcdefULL;
    for (const unsigned char *p = (const unsigned char *)event_id; p && *p; ++p)
        h1 = splitmix64(h1 ^ *p);
    uint64_t h2 = 0x0fedcba987654321ULL;
    for (const unsigned char *p = (const unsigned char *)seat_id; p && *p; ++p)
        h2 = splitmix64(h2 ^ *p);
    uint64_t h = h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1<<6) + (h1>>2));
    return h;
}

void tb_random_bytes_fast(unsigned char *out, size_t n)
{
    if (!out || n == 0) return;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    arc4random_buf(out, n);
    return;
#else
    FILE *urnd = fopen("/dev/urandom", "rb");
    if (urnd) {
        size_t readn = fread(out, 1, n, urnd);
        fclose(urnd);
        if (readn == n) return;
    }
    for (size_t i = 0; i < n; ++i)
        out[i] = (unsigned char)(rand() & 0xFF);
#endif
}
