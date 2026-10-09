#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

REPS="${REPS:-5}"
TH="${STUDY_THREADS:-12}"

PCPU_ORDER=(0 2 4 6 8 10 1 3 5 7 9 11 12 13 14 15)
PIN=""
if command -v taskset >/dev/null 2>&1; then
    out=""
    for ((i = 0; i < TH && i < ${#PCPU_ORDER[@]}; i++)); do
        out+="${out:+,}${PCPU_ORDER[i]}"
    done
    PIN="taskset -c $out"
fi

make -s ps build/map_omp build/vecmul_omp build/reduction_omp build/matmul_omp
mkdir -p results/paper_strategy
OUT=results/paper_strategy/strategy.csv
STATS=results/paper_strategy/stats.log
echo "benchmark,impl,dist,sched,vicinity,rep,seconds,checksum" > "$OUT"
: > "$STATS"

field() { grep -oP "(?<=$2=)[^ ]+" <<<"$1" | head -1 || true; }

CONFIGS=$(cat <<'EOF'
omp map - - - 63 32768 250
ps map coarse standard 0 63 32768 250
ps map coarse locality 0 63 32768 250
ps map coarse locality 1 63 32768 250
ps map coarse locality 2 63 32768 250
ps map fine locality 0 63 32768 250
omp vecmul - - - 128 28672 200
ps vecmul coarse standard 0 128 28672 200
ps vecmul coarse locality 0 128 28672 200
ps vecmul coarse locality 1 128 28672 200
ps vecmul coarse locality 2 128 28672 200
ps vecmul fine locality 0 128 28672 200
omp reduction - - - 33554432 10
ps reduction coarse standard 0 33554432 10
ps reduction coarse locality 0 33554432 10
ps reduction coarse locality 1 33554432 10
ps reduction coarse locality 2 33554432 10
ps reduction fine locality 0 33554432 10
omp matmul - - - 1024 64
ps matmul coarse standard 0 1024 64
ps matmul coarse locality 0 1024 64
ps matmul coarse locality 1 1024 64
ps matmul coarse locality 2 1024 64
ps matmul fine locality 0 1024 64
EOF
)

run_config() {
    local r=$1 impl=$2 bench=$3 dist=$4 sched=$5 vic=$6
    shift 6
    local line t cs tmp
    if [ "$impl" = "omp" ]; then
        line=$(OMP_NUM_THREADS="$TH" $PIN "./build/${bench}_omp" "$@" 2>/dev/null)
        t=$(field "$line" time)
        cs=$(field "$line" checksum)
        echo "$bench,omp,n/a,n/a,0,$r,$t,$cs" >> "$OUT"
        printf '  %-10s omp                      -> %ss\n' "$bench" "$t"
    else
        tmp=$(mktemp)
        line=$(PS_THREADS="$TH" PS_DIST="$dist" PS_SCHED="$sched" PS_VICINITY="$vic" PS_STATS=1 \
               "./build/${bench}_ps" "$@" 2>"$tmp")
        t=$(field "$line" time)
        cs=$(field "$line" checksum)
        echo "$bench,ps,$dist,$sched,$vic,$r,$t,$cs" >> "$OUT"
        sed "s/^/[$bench,$dist,$sched,vic=$vic] /" "$tmp" >> "$STATS"
        rm -f "$tmp"
        printf '  %-10s ps  %-8s %-9s vic=%-2s -> %ss\n' "$bench" "$dist" "$sched" "$vic" "$t"
    fi
}

# Reps outer, configs inner: transient slow windows on the machine affect all
# configurations equally, which keeps best-of-N comparisons fair.
for r in $(seq 1 "$REPS"); do
    echo "== rep $r / $REPS =="
    while read -r impl bench dist sched vic args; do
        if [ -n "$impl" ]; then
            run_config "$r" "$impl" "$bench" "$dist" "$sched" "$vic" $args
        fi
    done <<<"$CONFIGS"
done

echo "results: $OUT (stats: $STATS)"
