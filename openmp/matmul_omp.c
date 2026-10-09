#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "common.h"

static void NOINLINE fill(double *m, int n, uint64_t seed)
{
    uint64_t s = seed;
    for (int i = 0; i < n * n; i++)
        m[i] = frand01(&s);
}

static void NOINLINE matmul(double *restrict C, const double *restrict A,
                            const double *restrict B, int n, int bs)
{
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            C[i * n + j] = 0.0;

    #pragma omp parallel for collapse(2) schedule(static)
    for (int ii = 0; ii < n; ii += bs)
        for (int jj = 0; jj < n; jj += bs)
            for (int kk = 0; kk < n; kk += bs)
                for (int i = ii; i < ii + bs; i++)
                    for (int k = kk; k < kk + bs; k++) {
                        double a = A[i * n + k];
                        for (int j = jj; j < jj + bs; j++)
                            C[i * n + j] += a * B[k * n + j];
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

    double *A = malloc((size_t)n * n * sizeof *A);
    double *B = malloc((size_t)n * n * sizeof *B);
    double *C = malloc((size_t)n * n * sizeof *C);
    if (!A || !B || !C) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    fill(A, n, 12345);
    fill(B, n, 67890);

    double t0 = now_sec();
    matmul(C, A, B, n, bs);
    double t1 = now_sec();

    double sum = 0.0;
    for (int i = 0; i < n * n; i++)
        sum += C[i];

    printf("RESULT kernel=matmul impl=omp threads=%d n=%d bs=%d flops=%.0f time=%.6f checksum=%.15g\n",
           omp_get_max_threads(), n, bs, 2.0 * n * n * n, t1 - t0, sum);

    free(A);
    free(B);
    free(C);
    return 0;
}
