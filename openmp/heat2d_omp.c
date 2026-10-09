#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "common.h"

static void NOINLINE jacobi(double **up, double **vp, int n, int iters)
{
    double *u = *up;
    double *v = *vp;

    #pragma omp parallel
    {
        for (int t = 0; t < iters; t++) {
            #pragma omp for schedule(static)
            for (int i = 1; i < n - 1; i++)
                for (int j = 1; j < n - 1; j++)
                    v[i * n + j] = 0.25 * (u[(i - 1) * n + j] + u[(i + 1) * n + j]
                                         + u[i * n + j - 1] + u[i * n + j + 1]);
            #pragma omp single
            {
                double *tmp = u;
                u = v;
                v = tmp;
            }
        }
    }

    *up = u;
    *vp = v;
}

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 2048;
    int iters = argc > 2 ? atoi(argv[2]) : 200;
    if (n < 3 || iters < 0) {
        fprintf(stderr, "usage: %s n iters\n", argv[0]);
        return 1;
    }

    size_t cells = (size_t)n * n;
    double *u = malloc(cells * sizeof *u);
    double *v = malloc(cells * sizeof *v);
    if (!u || !v) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            u[i * n + j] = (i == 0 || j == 0 || i == n - 1 || j == n - 1)
                               ? 0.0
                               : (double)((i * 7 + j * 13) % 97) / 97.0;
    for (size_t i = 0; i < cells; i++)
        v[i] = 0.0;

    double t0 = now_sec();
    jacobi(&u, &v, n, iters);
    double t1 = now_sec();

    double sum = 0.0;
    for (size_t i = 0; i < cells; i++)
        sum += u[i];

    printf("RESULT kernel=heat2d impl=omp threads=%d n=%d iters=%d flops=%.0f time=%.6f checksum=%.15g\n",
           omp_get_max_threads(), n, iters, 5.0 * iters * (n - 2) * (n - 2), t1 - t0, sum);

    free(u);
    free(v);
    return 0;
}
