#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

REPS="${REPS:-3}"
THREADS="${THREADS:-1 2 4 8 12 16}"
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

PCPU_ORDER=(0 2 4 6 8 10 1 3 5 7 9 11 12 13 14 15)
PIN_DISABLE=""
command -v taskset >/dev/null 2>&1 || PIN_DISABLE=1

cpus_for() {
    local t=$1 out=""
    for ((i = 0; i < t && i < ${#PCPU_ORDER[@]}; i++)); do
        out+="${out:+,}${PCPU_ORDER[i]}"
    done
    echo "$out"
}

make -s all
mkdir -p results
OUT="results/timings.csv"
echo "kernel,impl,threads,param,rep,seconds,checksum,extra" > "$OUT"

field() {
    grep -oP "(?<=$2=)[^ ]+" <<<"$1" | head -1 || true
}

run_one() {
    local bin=$1 kernel=$2 impl=$3 th=$4 param=$5
    shift 5
    for r in $(seq 1 "$REPS"); do
        local line t cs ex
        if [ -n "${PIN_CPUS:-}" ] && [ -z "$PIN_DISABLE" ]; then
            line=$(OMP_NUM_THREADS="$th" taskset -c "$PIN_CPUS" "$bin" "$@")
        else
            line=$("$bin" "$@")
        fi
        t=$(field "$line" time)
        cs=$(field "$line" checksum)
        ex=$(field "$line" flops)
        [ -z "$ex" ] && ex=$(field "$line" inertia)
        echo "$kernel,$impl,$th,$param,$r,$t,$cs,$ex" >> "$OUT"
        printf '  %-9s %-4s threads=%-2s rep=%s time=%ss\n' "$kernel" "$impl" "$th" "$r" "$t"
    done
}

echo "== sequential runs =="
run_one build/matmul_seq matmul seq 1 "n=$MATMUL_N" "$MATMUL_N" "$MATMUL_BS"
run_one build/heat2d_seq heat2d seq 1 "n=$HEAT_N;iters=$HEAT_ITERS" "$HEAT_N" "$HEAT_ITERS"
run_one build/kmeans_seq kmeans seq 1 "np=$KMEANS_NP;k=$KMEANS_K;d=$KMEANS_D" "$KMEANS_NP" "$KMEANS_K" "$KMEANS_D" "$KMEANS_ITERS"
run_one build/map_seq map seq 1 "nv=$MAP_NV;len=$MAP_LEN;reps=$MAP_REPS" "$MAP_NV" "$MAP_LEN" "$MAP_REPS"
run_one build/reduction_seq reduction seq 1 "n=$RED_N;depth=$RED_DEPTH" "$RED_N" "$RED_DEPTH"
run_one build/sparselu_seq sparselu seq 1 "n=$SLU_N;bs=$SLU_BS" "$SLU_N" "$SLU_BS"
run_one build/vecmul_seq vecmul seq 1 "nv=$VEC_NV;len=$VEC_LEN;reps=$VEC_REPS" "$VEC_NV" "$VEC_LEN" "$VEC_REPS"

echo "== openmp runs (threads pinned: physical P-cores first, then SMT, then E-cores) =="
for th in $THREADS; do
    PIN_CPUS="$(cpus_for "$th")"
    run_one build/matmul_omp matmul omp "$th" "n=$MATMUL_N" "$MATMUL_N" "$MATMUL_BS"
    run_one build/heat2d_omp heat2d omp "$th" "n=$HEAT_N;iters=$HEAT_ITERS" "$HEAT_N" "$HEAT_ITERS"
    run_one build/kmeans_omp kmeans omp "$th" "np=$KMEANS_NP;k=$KMEANS_K;d=$KMEANS_D" "$KMEANS_NP" "$KMEANS_K" "$KMEANS_D" "$KMEANS_ITERS"
    run_one build/map_omp map omp "$th" "nv=$MAP_NV;len=$MAP_LEN;reps=$MAP_REPS" "$MAP_NV" "$MAP_LEN" "$MAP_REPS"
    run_one build/reduction_omp reduction omp "$th" "n=$RED_N;depth=$RED_DEPTH" "$RED_N" "$RED_DEPTH"
    run_one build/sparselu_omp sparselu omp "$th" "n=$SLU_N;bs=$SLU_BS" "$SLU_N" "$SLU_BS"
    run_one build/vecmul_omp vecmul omp "$th" "nv=$VEC_NV;len=$VEC_LEN;reps=$VEC_REPS" "$VEC_NV" "$VEC_LEN" "$VEC_REPS"
done

echo "results written to $OUT"
