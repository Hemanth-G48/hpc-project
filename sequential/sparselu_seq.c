#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "common.h"

static void lu_nopivot(double *A, int m)
{
    for (int k = 0; k < m; k++) {
        double d = A[k * m + k];
        for (int i = k + 1; i < m; i++) {
            double l = A[i * m + k] / d;
            A[i * m + k] = l;
            for (int j = k + 1; j < m; j++)
                A[i * m + j] -= l * A[k * m + j];
        }
    }
}

static void solve_lower_unit(const double *LU, double *B, int m)
{
    for (int j = 0; j < m; j++)
        for (int i = 0; i < m; i++) {
            double s = B[i * m + j];
            for (int t = 0; t < i; t++)
                s -= LU[i * m + t] * B[t * m + j];
            B[i * m + j] = s;
        }
}

static void solve_upper(const double *LU, double *B, int m)
{
    for (int i = 0; i < m; i++) {
        double *b = B + i * m;
        for (int j = 0; j < m; j++) {
            double s = b[j];
            for (int t = 0; t < j; t++)
                s -= b[t] * LU[t * m + j];
            b[j] = s / LU[j * m + j];
        }
    }
}

static void schur(double *D, const double *Lb, const double *Ub, int m)
{
    for (int i = 0; i < m; i++)
        for (int k = 0; k < m; k++) {
            double l = Lb[i * m + k];
            for (int j = 0; j < m; j++)
                D[i * m + j] -= l * Ub[k * m + j];
        }
}

static void fwd_sub_vec(const double *LU, double *b, int m)
{
    for (int i = 0; i < m; i++) {
        double s = b[i];
        for (int t = 0; t < i; t++)
            s -= LU[i * m + t] * b[t];
        b[i] = s;
    }
}

static void back_sub_vec(const double *LU, double *b, int m)
{
    for (int j = m - 1; j >= 0; j--) {
        double s = b[j];
        for (int t = j + 1; t < m; t++)
            s -= b[t] * LU[j * m + t];
        b[j] = s / LU[j * m + j];
    }
}

static void NOINLINE factorize(double *D, double *L, double *U, int nb, int bs)
{
    size_t bsz = (size_t)bs * bs;
    for (int k = 0; k < nb; k++) {
        double *Dk = D + (size_t)k * bsz;
        lu_nopivot(Dk, bs);
        if (k + 1 < nb) {
            double *Lk = L + (size_t)k * bsz;
            double *Uk = U + (size_t)k * bsz;
            solve_lower_unit(Dk, Uk, bs);
            solve_upper(Dk, Lk, bs);
            schur(D + (size_t)(k + 1) * bsz, Lk, Uk, bs);
        }
    }
}

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 8192;
    int bs = argc > 2 ? atoi(argv[2]) : 256;
    if (n <= 0 || bs <= 0 || n % bs != 0 || n / bs < 2) {
        fprintf(stderr, "usage: %s n bs   (n must be a multiple of bs)\n", argv[0]);
        return 1;
    }
    int nb = n / bs;
    size_t bsz = (size_t)bs * bs;

    double *D = malloc((size_t)nb * bsz * sizeof *D);
    double *L = malloc((size_t)(nb - 1) * bsz * sizeof *L);
    double *U = malloc((size_t)(nb - 1) * bsz * sizeof *U);
    double *b = malloc((size_t)n * sizeof *b);
    double *y = malloc((size_t)n * sizeof *y);
    if (!D || !L || !U || !b || !y) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    uint64_t s = 0x243f6a8885a308d3ULL;
    for (size_t i = 0; i < (size_t)nb * bsz; i++)
        D[i] = 2.0 * frand01(&s) - 1.0;
    for (size_t i = 0; i < (size_t)(nb - 1) * bsz; i++) {
        L[i] = 2.0 * frand01(&s) - 1.0;
        U[i] = 2.0 * frand01(&s) - 1.0;
    }

    for (int k = 0; k < nb; k++)
        for (int i = 0; i < bs; i++) {
            double rowsum = 0.0;
            for (int j = 0; j < bs; j++)
                rowsum += fabs(D[(size_t)k * bsz + i * bs + j]);
            if (k > 0)
                for (int j = 0; j < bs; j++)
                    rowsum += fabs(L[(size_t)(k - 1) * bsz + i * bs + j]);
            if (k + 1 < nb)
                for (int j = 0; j < bs; j++)
                    rowsum += fabs(U[(size_t)k * bsz + i * bs + j]);
            D[(size_t)k * bsz + i * bs + i] = rowsum + 1.0;
        }

    for (int k = 0; k < nb; k++) {
        for (int i = 0; i < bs; i++) {
            double r = 0.0;
            for (int j = 0; j < bs; j++)
                r += D[(size_t)k * bsz + i * bs + j];
            if (k > 0)
                for (int j = 0; j < bs; j++)
                    r += L[(size_t)(k - 1) * bsz + i * bs + j];
            if (k + 1 < nb)
                for (int j = 0; j < bs; j++)
                    r += U[(size_t)k * bsz + i * bs + j];
            b[(size_t)k * bs + i] = r;
        }
    }

    double t0 = now_sec();
    factorize(D, L, U, nb, bs);
    double t1 = now_sec();

    for (size_t i = 0; i < (size_t)n; i++)
        y[i] = b[i];
    for (int k = 0; k < nb; k++) {
        double *yk = y + (size_t)k * bs;
        if (k > 0) {
            const double *Lk = L + (size_t)(k - 1) * bsz;
            const double *yp = y + (size_t)(k - 1) * bs;
            for (int i = 0; i < bs; i++)
                for (int j = 0; j < bs; j++)
                    yk[i] -= Lk[i * bs + j] * yp[j];
        }
        fwd_sub_vec(D + (size_t)k * bsz, yk, bs);
    }
    for (int k = nb - 1; k >= 0; k--) {
        double *yk = y + (size_t)k * bs;
        if (k + 1 < nb) {
            const double *Uk = U + (size_t)k * bsz;
            const double *yn = y + (size_t)(k + 1) * bs;
            for (int i = 0; i < bs; i++)
                for (int j = 0; j < bs; j++)
                    yk[i] -= Uk[i * bs + j] * yn[j];
        }
        back_sub_vec(D + (size_t)k * bsz, yk, bs);
    }

    double resid = 0.0;
    for (int i = 0; i < n; i++) {
        double e = fabs(y[i] - 1.0);
        if (e > resid)
            resid = e;
    }

    double sum = 0.0;
    for (size_t i = 0; i < (size_t)nb * bsz; i++)
        sum += fabs(D[i]);
    for (size_t i = 0; i < (size_t)(nb - 1) * bsz; i++) {
        sum += fabs(L[i]);
        sum += fabs(U[i]);
    }

    printf("RESULT kernel=sparselu impl=seq threads=1 n=%d bs=%d resid=%.3g flops=%.0f time=%.6f checksum=%.15g\n",
           n, bs, resid, (double)nb * 3.667 * bs * bs * bs, t1 - t0, sum);

    free(D);
    free(L);
    free(U);
    free(b);
    free(y);
    return 0;
}
