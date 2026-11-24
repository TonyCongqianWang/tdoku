#!/bin/bash
# Repeat run_time.sh in independent processes (fresh address-space layout each time) to expose
# per-process layout effects, which interleaving inside one process cannot average out.
#   research/scripts/run_time_multi.sh <split> <cpu> <procs> <reps> <out_prefix> <spec> [limit] [seed]
set -euo pipefail
split=$1; cpu=$2; procs=$3; reps=$4; out=$5; spec=$6; limit=${7:-2}; seed=${8:-1}
root=$(cd "$(dirname "$0")/../.." && pwd)
for p in $(seq 1 "$procs"); do
  "$root"/research/scripts/run_time.sh "$split" "$cpu" "$reps" "$out.proc$p.csv" "$spec" "$limit" "$seed"
done
