#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "ps_runtime.h"

static int **g_blk;
static int *g_id;
static int vlen;

static void NOINLINE vecmul_task(void *arg)
{
    int k = (int)(intptr_t)arg;
    const int *xk = g_blk[k];
    const int *yk = xk + vlen;
    int *zk = g_blk[k] + 2 * vlen;
    for (int j = 0; j < vlen; j++)
        zk[j] = xk[j] * yk[j];
}

int main(int argc, char **argv)
{
    int nv = argc > 1 ? atoi(argv[1]) : 128;
    int len = argc > 2 ? atoi(argv[2]) : 28672;
    int reps = argc > 3 ? atoi(argv[3]) : 200;

    vlen = len;
    ps_init();
    ps_policy_t pol = ps_env_policy();
    g_blk = malloc((size_t)nv * sizeof *g_blk);
    g_id = malloc((size_t)nv * sizeof *g_id);
    if (!g_blk || !g_id) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    for (int k = 0; k < nv; k++) {
        g_id[k] = ps_alloc((size_t)3 * len * sizeof(int), pol);
        g_blk[k] = ps_ptr(g_id[k]);
    }

    uint64_t s = 0x243f6a8885a308d3ULL;
    for (int k = 0; k < nv; k++) {
        int *xk = g_blk[k];
        int *yk = xk + len;
        for (int j = 0; j < len; j++) {
            xk[j] = (int)(xorshift64(&s) % 1000);
            yk[j] = (int)(xorshift64(&s) % 1000);
        }
    }

    double t0 = now_sec();
    for (int r = 0; r < reps; r++) {
        for (int k = 0; k < nv; k++) {
            ps_dep_t deps[3] = {
                { g_id[k], 0, (size_t)len * sizeof(int), 0.0 },
                { g_id[k], (size_t)len * sizeof(int), (size_t)len * sizeof(int), 0.0 },
                { g_id[k], (size_t)2 * len * sizeof(int), (size_t)len * sizeof(int), 0.0 },
            };
            ps_task(vecmul_task, (void *)(intptr_t)k, deps, 3);
        }
        ps_wait();
    }
    double t1 = now_sec();

    uint64_t sum = 0;
    for (int k = 0; k < nv; k++) {
        int *zk = g_blk[k] + 2 * len;
        for (int j = 0; j < len; j++)
            sum += (uint64_t)zk[j];
    }

    printf("RESULT kernel=vecmul_ps impl=ps sched=%s dist=%s nv=%d len=%d reps=%d threads=%d time=%.6f checksum=%llu\n",
           ps_sched_name(), ps_dist_name(), nv, len, reps, ps_nthreads(), t1 - t0,
           (unsigned long long)sum);

    ps_shutdown();
    return 0;
}
