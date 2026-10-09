#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "ps_runtime.h"

static int g_n, g_bs, g_nb;
static double *A, *B, *C;
static int g_aid, g_bid, g_cid;

static void NOINLINE mm_task(void *arg)
{
    int idx = (int)(intptr_t)arg;
    int ii = idx / g_nb;
    int jj = idx % g_nb;
    int n = g_n, bs = g_bs;

    double *Cc = C + (size_t)ii * bs * n + (size_t)jj * bs;
    for (int i = 0; i < bs; i++)
        for (int j = 0; j < bs; j++)
            Cc[(size_t)i * n + j] = 0.0;

    for (int kk = 0; kk < n; kk += bs)
        for (int i = ii * bs; i < ii * bs + bs; i++)
            for (int k = kk; k < kk + bs; k++) {
                double a = A[(size_t)i * n + k];
                double *Cr = C + (size_t)i * n + jj * bs;
                const double *Br = B + (size_t)k * n + jj * bs;
                for (int j = 0; j < bs; j++)
                    Cr[j] += a * Br[j];
            }
}

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 1024;
    int bs = argc > 2 ? atoi(argv[2]) : 64;
    if (n <= 0 || bs <= 0 || n % bs != 0) {
        fprintf(stderr, "usage: %s n bs   (n must be a multiple of bs)\n", argv[0]);
        return 1;
    }

    g_n = n;
    g_bs = bs;
    g_nb = n / bs;

    ps_init();
    ps_policy_t pol = ps_env_policy();
    g_aid = ps_alloc((size_t)n * n * sizeof(double), pol);
    g_bid = ps_alloc((size_t)n * n * sizeof(double), pol);
    g_cid = ps_alloc((size_t)n * n * sizeof(double), pol);
    A = ps_ptr(g_aid);
    B = ps_ptr(g_bid);
    C = ps_ptr(g_cid);

    uint64_t s = 12345;
    for (size_t i = 0; i < (size_t)n * n; i++)
        A[i] = frand01(&s);
    s = 67890;
    for (size_t i = 0; i < (size_t)n * n; i++)
        B[i] = frand01(&s);

    double t0 = now_sec();
    for (int ii = 0; ii < g_nb; ii++)
        for (int jj = 0; jj < g_nb; jj++) {
            ps_dep_t deps[3] = {
                { g_cid, ((size_t)ii * bs * n + jj * bs) * sizeof(double),
                  (size_t)bs * bs * sizeof(double), 0.0 },
                { g_aid, (size_t)ii * bs * n * sizeof(double),
                  (size_t)bs * n * sizeof(double), 1.0 },
                { g_bid, (size_t)jj * bs * sizeof(double),
                  (size_t)n * bs * sizeof(double), 0.0 },
            };
            ps_task(mm_task, (void *)(intptr_t)(ii * g_nb + jj), deps, 3);
        }
    ps_wait();
    double t1 = now_sec();

    double sum = 0.0;
    for (size_t i = 0; i < (size_t)n * n; i++)
        sum += C[i];

    printf("RESULT kernel=matmul_ps impl=ps sched=%s dist=%s n=%d bs=%d threads=%d time=%.6f checksum=%.15g\n",
           ps_sched_name(), ps_dist_name(), n, bs, ps_nthreads(), t1 - t0, sum);

    ps_shutdown();
    return 0;
}
