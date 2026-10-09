#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "omp_paper.h"

static int g_n, g_bs, g_nb;
static double *A, *B, *C;

static void NOINLINE mm_block(int idx)
{
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

    op_init();
    A = op_alloc((size_t)n * n * sizeof(double));
    B = op_alloc((size_t)n * n * sizeof(double));
    C = op_alloc((size_t)n * n * sizeof(double));

    uint64_t s = 12345;
    for (size_t i = 0; i < (size_t)n * n; i++)
        A[i] = frand01(&s);
    s = 67890;
    for (size_t i = 0; i < (size_t)n * n; i++)
        B[i] = frand01(&s);

    double t0 = now_sec();
    #pragma omp parallel
    {
        #pragma omp single
        {
            for (int ii = 0; ii < g_nb; ii++)
                for (int jj = 0; jj < g_nb; jj++) {
                    int idx = ii * g_nb + jj;
                    size_t offc = (size_t)ii * bs * n + (size_t)jj * bs;
                    #pragma omp task depend(in : A[0:(size_t)n * n], B[0:(size_t)n * n]) \
                        affinity(C[offc:1]) firstprivate(idx)
                    mm_block(idx);
                }
        }
    }
    double t1 = now_sec();

    double sum = 0.0;
    for (size_t i = 0; i < (size_t)n * n; i++)
        sum += C[i];

    printf("RESULT kernel=matmul_tasks impl=tasks dist=%s n=%d bs=%d threads=%d time=%.6f checksum=%.15g\n",
           op_dist_name(), n, bs, omp_get_max_threads(), t1 - t0, sum);

    op_stats();
    return 0;
}
