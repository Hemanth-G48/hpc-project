#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "ps_runtime.h"

static double alpha = 0.9999999;
static double **g_vec;
static int *g_id;
static int vlen;

static void NOINLINE map_task(void *arg)
{
    int k = (int)(intptr_t)arg;
    double *vec = g_vec[k];
    for (int j = 0; j < vlen; j++)
        vec[j] *= alpha;
}

int main(int argc, char **argv)
{
    int nv = argc > 1 ? atoi(argv[1]) : 63;
    int len = argc > 2 ? atoi(argv[2]) : 32768;
    int reps = argc > 3 ? atoi(argv[3]) : 250;

    vlen = len;
    ps_init();
    ps_policy_t pol = ps_env_policy();
    g_vec = malloc((size_t)nv * sizeof *g_vec);
    g_id = malloc((size_t)nv * sizeof *g_id);
    if (!g_vec || !g_id) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    for (int k = 0; k < nv; k++) {
        g_id[k] = ps_alloc((size_t)len * sizeof(double), pol);
        g_vec[k] = ps_ptr(g_id[k]);
    }

    uint64_t s = 0x243f6a8885a308d3ULL;
    for (int k = 0; k < nv; k++)
        for (int j = 0; j < len; j++)
            g_vec[k][j] = frand01(&s);

    double t0 = now_sec();
    for (int r = 0; r < reps; r++) {
        for (int k = 0; k < nv; k++) {
            ps_dep_t d = { g_id[k], 0, (size_t)len * sizeof(double), 0.0 };
            ps_task(map_task, (void *)(intptr_t)k, &d, 1);
        }
        ps_wait();
    }
    double t1 = now_sec();

    double sum = 0.0;
    for (int k = 0; k < nv; k++)
        for (int j = 0; j < len; j++)
            sum += g_vec[k][j];

    printf("RESULT kernel=map_ps impl=ps sched=%s dist=%s nv=%d len=%d reps=%d threads=%d time=%.6f checksum=%.15g\n",
           ps_sched_name(), ps_dist_name(), nv, len, reps, ps_nthreads(), t1 - t0, sum);

    ps_shutdown();
    return 0;
}
