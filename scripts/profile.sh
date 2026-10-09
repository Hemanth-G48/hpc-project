#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

MATMUL_N="${MATMUL_N:-2048}"
MATMUL_BS="${MATMUL_BS:-64}"
HEAT_N="${HEAT_N:-2048}"
HEAT_ITERS="${HEAT_ITERS:-400}"
KMEANS_NP="${KMEANS_NP:-1000000}"
KMEANS_K="${KMEANS_K:-16}"
KMEANS_D="${KMEANS_D:-2}"
KMEANS_ITERS="${KMEANS_ITERS:-100}"
MAP_NV="${MAP_NV:-48}"
MAP_LEN="${MAP_LEN:-131072}"
MAP_REPS="${MAP_REPS:-100}"
RED_N="${RED_N:-33554432}"
RED_DEPTH="${RED_DEPTH:-10}"
SLU_N="${SLU_N:-8192}"
SLU_BS="${SLU_BS:-256}"
VEC_NV="${VEC_NV:-128}"
VEC_LEN="${VEC_LEN:-7168}"
VEC_REPS="${VEC_REPS:-1000}"

make -s all
mkdir -p profiling

profile_kernel() {
    local kernel=$1 src=$2
    shift 2
    local bin="build/${kernel}_seq"

    echo "== $kernel =="
    if perf record -q -o "profiling/$kernel.perf.data" -e cpu-clock -g --call-graph fp -- "$bin" "$@" >/dev/null 2>&1; then
        perf stat -d -- "$bin" "$@" >/dev/null 2>"profiling/$kernel.perfstat.txt" || true
        perf report -i "profiling/$kernel.perf.data" --stdio --no-children 2>/dev/null \
            > "profiling/$kernel.report.full.txt" || true
        grep -vE '^#' "profiling/$kernel.report.full.txt" | head -30 \
            > "profiling/$kernel.hotspots.txt" || true
        perf annotate -i "profiling/$kernel.perf.data" --stdio 2>/dev/null \
            > "profiling/$kernel.annotate.full.txt" || true
        head -60 "profiling/$kernel.annotate.full.txt" \
            > "profiling/$kernel.annotate.txt" || true
        echo "  perf profiles written"
    else
        echo "  perf unavailable, falling back to gprof"
        gcc -O3 -g -pg -std=gnu11 -I. "$src" -o "build/${kernel}_pg" -lm
        ( cd profiling \
          && "../build/${kernel}_pg" "$@" > /dev/null \
          && gprof -b "../build/${kernel}_pg" gmon.out | head -40 > "${kernel}.hotspots.txt" )
        echo "  gprof profile written"
    fi
}

profile_kernel matmul "sequential/matmul_seq.c" "$MATMUL_N" "$MATMUL_BS"
profile_kernel heat2d "sequential/heat2d_seq.c" "$HEAT_N" "$HEAT_ITERS"
profile_kernel kmeans "sequential/kmeans_seq.c" "$KMEANS_NP" "$KMEANS_K" "$KMEANS_D" "$KMEANS_ITERS"
profile_kernel map "sequential/map_seq.c" "$MAP_NV" "$MAP_LEN" "$MAP_REPS"
profile_kernel reduction "sequential/reduction_seq.c" "$RED_N" "$RED_DEPTH"
profile_kernel sparselu "sequential/sparselu_seq.c" "$SLU_N" "$SLU_BS"
profile_kernel vecmul "sequential/vecmul_seq.c" "$VEC_NV" "$VEC_LEN" "$VEC_REPS"

echo "profiles written to profiling/"
