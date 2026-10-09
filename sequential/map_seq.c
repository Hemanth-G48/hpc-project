#include <stdio.h>
#include <stdlib.h>
#include "common.h"

static void NOINLINE map_kernel(double *v, int nv, int len, int reps, double alpha)
{
    for (int r = 0; r < reps; r++)
        for (int k = 0; k < nv; k++) {
            double *vec = v + (size_t)k * len;
            for (int j = 0; j < len; j++)
                vec[j] *= alpha;
        }
}

int main(int argc, char **argv)
{
    int nv = argc > 1 ? atoi(argv[1]) : 48;
    int len = argc > 2 ? atoi(argv[2]) : 131072;
    int reps = argc > 3 ? atoi(argv[3]) : 100;
    double alpha = 0.9999999;
    if (nv <= 0 || len <= 0 || reps <= 0) {
        fprintf(stderr, "usage: %s nv len reps\n", argv[0]);
        return 1;
    }

    size_t total = (size_t)nv * (size_t)len;
    double *v = malloc(total * sizeof *v);
    if (!v) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    uint64_t s = 0x243f6a8885a308d3ULL;
    for (size_t i = 0; i < total; i++)
        v[i] = frand01(&s);

    double t0 = now_sec();
    map_kernel(v, nv, len, reps, alpha);
    double t1 = now_sec();

    double sum = 0.0;
    for (size_t i = 0; i < total; i++)
        sum += v[i];

    printf("RESULT kernel=map impl=seq threads=1 nv=%d len=%d reps=%d flops=%.0f time=%.6f checksum=%.15g\n",
           nv, len, reps, (double)total * reps, t1 - t0, sum);

    free(v);
    return 0;
}
