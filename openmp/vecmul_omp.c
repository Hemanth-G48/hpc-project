#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "common.h"

static void NOINLINE vecmul_kernel(const int *x, const int *y, int *z,
                                   int nv, int len, int reps)
{
    #pragma omp parallel
    {
        for (int r = 0; r < reps; r++) {
            #pragma omp for schedule(static)
            for (int k = 0; k < nv; k++) {
                const int *xk = x + (size_t)k * len;
                const int *yk = y + (size_t)k * len;
                int *zk = z + (size_t)k * len;
                for (int j = 0; j < len; j++)
                    zk[j] = xk[j] * yk[j];
            }
        }
    }
}

int main(int argc, char **argv)
{
    int nv = argc > 1 ? atoi(argv[1]) : 128;
    int len = argc > 2 ? atoi(argv[2]) : 7168;
    int reps = argc > 3 ? atoi(argv[3]) : 1000;
    if (nv <= 0 || len <= 0 || reps <= 0) {
        fprintf(stderr, "usage: %s nv len reps\n", argv[0]);
        return 1;
    }

    size_t total = (size_t)nv * (size_t)len;
    int *x = malloc(total * sizeof *x);
    int *y = malloc(total * sizeof *y);
    int *z = malloc(total * sizeof *z);
    if (!x || !y || !z) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    uint64_t s = 0x243f6a8885a308d3ULL;
    for (size_t i = 0; i < total; i++) {
        x[i] = (int)(xorshift64(&s) % 1000);
        y[i] = (int)(xorshift64(&s) % 1000);
    }

    double t0 = now_sec();
    vecmul_kernel(x, y, z, nv, len, reps);
    double t1 = now_sec();

    uint64_t sum = 0;
    for (size_t i = 0; i < total; i++)
        sum += (uint64_t)z[i];

    printf("RESULT kernel=vecmul impl=omp threads=%d nv=%d len=%d reps=%d flops=%.0f time=%.6f checksum=%llu\n",
           omp_get_max_threads(), nv, len, reps, (double)total * reps, t1 - t0,
           (unsigned long long)sum);

    free(x);
    free(y);
    free(z);
    return 0;
}
