#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

static uint64_t fnv1a_u64(uint64_t h, uint64_t v)
{
    h ^= v;
    h *= 1099511628211ULL;
    return h;
}

static int NOINLINE iterate(const double *pts, double *cen, double *sum, int *cnt, int *lab,
                            int np, int k, int d, int iters, double *inertia_out)
{
    int actual = 0;
    double inertia = 0.0;

    for (int it = 0; it < iters; it++) {
        int moved = 0;
        inertia = 0.0;

        for (int i = 0; i < np; i++) {
            const double *p = pts + (size_t)i * d;
            int best = 0;
            double bestd = 0.0;
            for (int t = 0; t < d; t++) {
                double diff = p[t] - cen[t];
                bestd += diff * diff;
            }
            for (int c = 1; c < k; c++) {
                double dist = 0.0;
                for (int t = 0; t < d; t++) {
                    double diff = p[t] - cen[(size_t)c * d + t];
                    dist += diff * diff;
                }
                if (dist < bestd) {
                    bestd = dist;
                    best = c;
                }
            }
            inertia += bestd;
            if (lab[i] != best) {
                lab[i] = best;
                moved++;
            }
        }

        memset(sum, 0, (size_t)k * d * sizeof *sum);
        memset(cnt, 0, (size_t)k * sizeof *cnt);
        for (int i = 0; i < np; i++) {
            int c = lab[i];
            cnt[c]++;
            for (int t = 0; t < d; t++)
                sum[(size_t)c * d + t] += pts[(size_t)i * d + t];
        }

        for (int c = 0; c < k; c++)
            if (cnt[c] > 0)
                for (int t = 0; t < d; t++)
                    cen[(size_t)c * d + t] = sum[(size_t)c * d + t] / cnt[c];

        actual = it + 1;
        if (moved == 0)
            break;
    }

    *inertia_out = inertia;
    return actual;
}

int main(int argc, char **argv)
{
    int np = argc > 1 ? atoi(argv[1]) : 1000000;
    int k = argc > 2 ? atoi(argv[2]) : 16;
    int d = argc > 3 ? atoi(argv[3]) : 2;
    int iters = argc > 4 ? atoi(argv[4]) : 100;
    if (np <= 0 || k <= 0 || k > np || d <= 0 || iters <= 0) {
        fprintf(stderr, "usage: %s np k d iters\n", argv[0]);
        return 1;
    }

    double *pts = malloc((size_t)np * d * sizeof *pts);
    double *cen = malloc((size_t)k * d * sizeof *cen);
    double *sum = malloc((size_t)k * d * sizeof *sum);
    int *cnt = malloc((size_t)k * sizeof *cnt);
    int *lab = malloc((size_t)np * sizeof *lab);
    if (!pts || !cen || !sum || !cnt || !lab) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    uint64_t s = 0x9e3779b97f4a7c15ULL;
    for (int c = 0; c < k; c++)
        for (int t = 0; t < d; t++)
            cen[(size_t)c * d + t] = 100.0 * frand01(&s);

    for (int i = 0; i < np; i++) {
        int c = i % k;
        for (int t = 0; t < d; t++)
            pts[(size_t)i * d + t] = cen[(size_t)c * d + t]
                + 8.0 * (frand01(&s) + frand01(&s) + frand01(&s) - 1.5);
    }

    for (int i = 0; i < np; i++)
        lab[i] = i % k;
    for (int c = 0; c < k; c++)
        for (int t = 0; t < d; t++)
            cen[(size_t)c * d + t] = pts[(size_t)c * d + t];

    double inertia = 0.0;
    double t0 = now_sec();
    int actual = iterate(pts, cen, sum, cnt, lab, np, k, d, iters, &inertia);
    double t1 = now_sec();

    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < np; i++)
        h = fnv1a_u64(h, (uint64_t)lab[i]);

    printf("RESULT kernel=kmeans impl=seq threads=1 np=%d k=%d d=%d iters=%d time=%.6f inertia=%.15g labelhash=%llu\n",
           np, k, d, actual, t1 - t0, inertia, (unsigned long long)h);

    free(pts);
    free(cen);
    free(sum);
    free(cnt);
    free(lab);
    return 0;
}
