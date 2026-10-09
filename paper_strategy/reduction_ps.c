#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "ps_runtime.h"

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
static int g_cid, g_gid;
static unsigned long g_run;

static void NOINLINE merge_task(void *arg)
{
    unsigned long start = (unsigned long)(uintptr_t)arg;
    merge_runs(g_cur + start, g_run, g_cur + start + g_run, g_run, g_alt + start);
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

    ps_init();
    ps_policy_t pol = ps_env_policy();
    int aid = ps_alloc(n * sizeof(double), pol);
    int bid = ps_alloc(n * sizeof(double), pol);
    double *a = ps_ptr(aid);
    double *b = ps_ptr(bid);

    uint64_t s = 0x243f6a8885a308d3ULL;
    for (unsigned long r = 0; r < runs; r++)
        for (unsigned long i = 0; i < run_size; i++)
            a[r * run_size + i] = (double)(i * 1024 + r) * 0.001
                                + frand01(&s) * 0.0005;

    g_cur = a;
    g_alt = b;
    g_cid = aid;
    g_gid = bid;
    g_run = run_size;

    double t0 = now_sec();
    for (int level = 0; level < depth; level++) {
        unsigned long span = 2 * g_run;
        for (unsigned long start = 0; start < n; start += span) {
            ps_dep_t deps[2] = {
                { g_cid, start * sizeof(double), span * sizeof(double), 0.0 },
                { g_gid, start * sizeof(double), span * sizeof(double), 0.0 },
            };
            ps_task(merge_task, (void *)(uintptr_t)start, deps, 2);
        }
        ps_wait();
        double *t = g_cur;
        g_cur = g_alt;
        g_alt = t;
        int ti = g_cid;
        g_cid = g_gid;
        g_gid = ti;
        g_run = span;
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

    printf("RESULT kernel=reduction_ps impl=ps sched=%s dist=%s n=%lu depth=%d sorted=%d threads=%d time=%.6f checksum=%.15g\n",
           ps_sched_name(), ps_dist_name(), n, depth, sorted, ps_nthreads(), t1 - t0, sum);

    ps_shutdown();
    return 0;
}
