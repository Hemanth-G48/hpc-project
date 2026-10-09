#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "omp_paper.h"

static int **g_blk;

static void NOINLINE vmul_block(int *b, int len)
{
    int *xk = b;
    int *yk = b + len;
    int *zk = b + 2 * len;
    for (int j = 0; j < len; j++)
        zk[j] = xk[j] * yk[j];
}

int main(int argc, char **argv)
{
    int nv = argc > 1 ? atoi(argv[1]) : 128;
    int len = argc > 2 ? atoi(argv[2]) : 28672;
    int reps = argc > 3 ? atoi(argv[3]) : 200;

    op_init();
    g_blk = malloc((size_t)nv * sizeof *g_blk);
    if (!g_blk) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    for (int k = 0; k < nv; k++)
        g_blk[k] = op_alloc((size_t)3 * len * sizeof(int));

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
    #pragma omp parallel
    {
        #pragma omp single
        {
            for (int r = 0; r < reps; r++) {
                for (int k = 0; k < nv; k++) {
                    int *b = g_blk[k];
                    #pragma omp task depend(in : b[0:len], b[len:len]) \
                        depend(out : b[2 * len:len]) affinity(b[2 * len:len]) firstprivate(b)
                    vmul_block(b, len);
                }
                #pragma omp taskwait
            }
        }
    }
    double t1 = now_sec();

    uint64_t sum = 0;
    for (int k = 0; k < nv; k++) {
        int *zk = g_blk[k] + 2 * len;
        for (int j = 0; j < len; j++)
            sum += (uint64_t)zk[j];
    }

    printf("RESULT kernel=vecmul_tasks impl=tasks dist=%s nv=%d len=%d reps=%d threads=%d time=%.6f checksum=%llu\n",
           op_dist_name(), nv, len, reps, omp_get_max_threads(), t1 - t0,
           (unsigned long long)sum);

    op_stats();
    return 0;
}
