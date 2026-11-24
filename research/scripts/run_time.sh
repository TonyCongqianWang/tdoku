#!/bin/bash
# Interleaved timing of variants on every category of a split, pinned to one CPU.
#   research/scripts/run_time.sh <split> <cpu> <reps> <out_file> <variant_spec> [limit] [seed]
set -euo pipefail
split=$1; cpu=$2; reps=$3; out=$4; spec=$5; limit=${6:-2}; seed=${7:-1}
root=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$(dirname "$out")"
: > "$out"
for f in "$root"/research/data/"$split"/*.txt; do
  taskset -c "$cpu" "${BENCH_BIN:-$root/build/research_bench}" -m time -l "$limit" -x "$seed" -r "$reps" -w 1 \
     -t "$split" -v "$spec" "$f" 2>/dev/null >> "$out"
done
