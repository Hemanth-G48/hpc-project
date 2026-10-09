#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "omp_paper.h"

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

static double *g_cur, *g_alt;
static unsigned long g_run;

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

    op_init();
    double *a = op_alloc(n * sizeof *a);
    double *b = op_alloc(n * sizeof *b);

    uint64_t s = 0x243f6a8885a308d3ULL;
    for (unsigned long r = 0; r < runs; r++)
        for (unsigned long i = 0; i < run_size; i++)
            a[r * run_size + i] = (double)(i * 1024 + r) * 0.001
                                + frand01(&s) * 0.0005;

    g_cur = a;
    g_alt = b;
    g_run = run_size;

    double t0 = now_sec();
    #pragma omp parallel
    {
        #pragma omp single
        {
            for (int level = 0; level < depth; level++) {
                unsigned long span = 2 * g_run;
                for (unsigned long start = 0; start < n; start += span) {
                    double *src = g_cur;
                    double *dst = g_alt;
                    unsigned long run = g_run;
                    #pragma omp task depend(in : src[start:span]) \
                        depend(out : dst[start:span]) affinity(dst[start:span]) \
                        firstprivate(src, dst, run, start, span)
                    merge_runs(src + start, run, src + start + run, run, dst + start);
                }
                #pragma omp taskwait
                double *t = g_cur;
                g_cur = g_alt;
                g_alt = t;
                g_run = span;
            }
        }
    }
    double t1 = now_sec();

    int sorted = 1;
    for (unsigned long i = 1; i < n; i++)
        if (g_cur[i] < g_cur[i - 1]) {
            sorted = 0;
            break;
        }

    double sum = 0.0;
    for (unsigned long i = 0; i < n; i++)
        sum += g_cur[i];

    printf("RESULT kernel=reduction_tasks impl=tasks dist=%s n=%lu depth=%d sorted=%d threads=%d time=%.6f checksum=%.15g\n",
           op_dist_name(), n, depth, sorted, omp_get_max_threads(), t1 - t0, sum);

    op_stats();
    return 0;
}
