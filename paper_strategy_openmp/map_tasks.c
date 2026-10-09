#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "omp_paper.h"

static double alpha = 0.9999999;
static double **g_vec;

static void NOINLINE scale_vec(double *v, int len)
{
    for (int j = 0; j < len; j++)
        v[j] *= alpha;
}

int main(int argc, char **argv)
{
    int nv = argc > 1 ? atoi(argv[1]) : 63;
    int len = argc > 2 ? atoi(argv[2]) : 32768;
    int reps = argc > 3 ? atoi(argv[3]) : 250;

    op_init();
    g_vec = malloc((size_t)nv * sizeof *g_vec);
    if (!g_vec) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    for (int k = 0; k < nv; k++)
        g_vec[k] = op_alloc((size_t)len * sizeof(double));

    uint64_t s = 0x243f6a8885a308d3ULL;
    for (int k = 0; k < nv; k++)
        for (int j = 0; j < len; j++)
            g_vec[k][j] = frand01(&s);

    double t0 = now_sec();
    #pragma omp parallel
    {
        #pragma omp single
        {
            for (int r = 0; r < reps; r++) {
                for (int k = 0; k < nv; k++) {
                    double *v = g_vec[k];
                    #pragma omp task depend(inout : v[0:len]) affinity(v[0:len]) firstprivate(v)
                    scale_vec(v, len);
                }
                #pragma omp taskwait
            }
        }
    }
    double t1 = now_sec();

    double sum = 0.0;
    for (int k = 0; k < nv; k++)
        for (int j = 0; j < len; j++)
            sum += g_vec[k][j];

    printf("RESULT kernel=map_tasks impl=tasks dist=%s nv=%d len=%d reps=%d threads=%d time=%.6f checksum=%.15g\n",
           op_dist_name(), nv, len, reps, omp_get_max_threads(), t1 - t0, sum);

    op_stats();
    return 0;
}
