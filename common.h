#ifndef HPC_COMMON_H
#define HPC_COMMON_H

#include <stdint.h>
#include <time.h>

#define NOINLINE __attribute__((noinline))

static inline double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static inline uint64_t xorshift64(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

static inline double frand01(uint64_t *s)
{
    return (double)(xorshift64(s) >> 11) * (1.0 / 9007199254740992.0);
}

#endif
