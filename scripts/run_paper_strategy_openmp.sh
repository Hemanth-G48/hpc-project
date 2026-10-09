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

make -s tasks build/map_seq build/vecmul_seq build/reduction_seq build/matmul_seq
mkdir -p results/paper_strategy_openmp
OUT=results/paper_strategy_openmp/tasks.csv
echo "benchmark,impl,rep,seconds,checksum" > "$OUT"

field() { grep -oP "(?<=$2=)[^ ]+" <<<"$1" | head -1 || true; }

run_tasks() {
    local bench=$1
    shift
    local want r line t cs
    want=$(./build/${bench}_seq "$@" | grep -oP '(?<=checksum=)[0-9.e+-]+')
    for r in $(seq 1 "$REPS"); do
        line=$(OMP_NUM_THREADS="$TH" $PIN "./build/${bench}_tasks" "$@" 2>/dev/null)
        t=$(field "$line" time)
        cs=$(field "$line" checksum)
        if [ "$cs" != "$want" ]; then
            echo "CHECKSUM MISMATCH ${bench}: got $cs want $want" >&2
            exit 1
        fi
        echo "$bench,tasks,$r,$t,$cs" >> "$OUT"
        printf '  %-10s rep=%s time=%ss  (checksum ok)\n' "$bench" "$r" "$t"
    done
}

run_tasks map 63 32768 250
run_tasks vecmul 128 28672 200
run_tasks reduction 33554432 10
run_tasks matmul 1024 64

echo "results written to $OUT"
