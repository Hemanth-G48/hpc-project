#include <stdio.h>
#include <stdlib.h>
#include "common.h"

static void merge_runs(const double *a, unsigned long n1,
                       const double *b, unsigned long n2, double *out)
{
    unsigned long i = 0, j = 0, k = 0;
    while (i < n1 && j < n2)
        out[k++] = (a[i] <= b[j]) ? a[i++] : b[j++];
    while (i < n1)
        out[k++] = a[i++];
    while (j < n2)
        out[k++] = b[j++];
}

static void NOINLINE reduce_kernel(double **ap, double **bp, unsigned long n, int depth)
{
    double *cur = *ap;
    double *alt = *bp;
    unsigned long run = n >> depth;

    for (int level = 0; level < depth; level++) {
        unsigned long span = 2 * run;
        for (unsigned long start = 0; start < n; start += span)
            merge_runs(cur + start, run, cur + start + run, run, alt + start);
        double *t = cur;
        cur = alt;
        alt = t;
        run = span;
    }

    *ap = cur;
    *bp = alt;
}

int main(int argc, char **argv)
{
    unsigned long n = argc > 1 ? strtoul(argv[1], NULL, 10) : 33554432UL;
    int depth = argc > 2 ? atoi(argv[2]) : 10;
    if (depth < 1 || depth > 30 || n == 0 || n % (1UL << depth) != 0) {
        fprintf(stderr, "usage: %s n depth   (n must be a multiple of 2^depth)\n", argv[0]);
        return 1;
    }

    unsigned long runs = 1UL << depth;
    unsigned long run_size = n / runs;

    double *a = malloc(n * sizeof *a);
    double *b = malloc(n * sizeof *b);
    if (!a || !b) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    uint64_t s = 0x243f6a8885a308d3ULL;
    for (unsigned long r = 0; r < runs; r++)
        for (unsigned long i = 0; i < run_size; i++)
            a[r * run_size + i] = (double)(i * 1024 + r) * 0.001
                                + frand01(&s) * 0.0005;

    double t0 = now_sec();
    reduce_kernel(&a, &b, n, depth);
    double t1 = now_sec();

    int sorted = 1;
    for (unsigned long i = 1; i < n; i++)
        if (a[i] < a[i - 1]) {
            sorted = 0;
            break;
        }

    double sum = 0.0;
    for (unsigned long i = 0; i < n; i++)
        sum += a[i];

    printf("RESULT kernel=reduction impl=seq threads=1 n=%lu depth=%d sorted=%d flops=%.0f time=%.6f checksum=%.15g\n",
           n, depth, sorted, (double)n * depth, t1 - t0, sum);

    free(a);
    free(b);
    return 0;
}
