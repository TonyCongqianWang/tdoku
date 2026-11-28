#!/bin/bash
# End-to-end timing on the validation split: run_time.sh <cpu> <out>   (check the load first)
cd "$(dirname "$0")"
cpu=$1; out=$2
: > "$out"
for limit in 2 1; do
  for s in hard hard11 hard1106 magictour 17clue unbiased pm pm_easy; do
    echo "## set $s limit $limit cpu $cpu" >> "$out"
    taskset -c "$cpu" ./time_e2e sets/val/$s.txt 100000 $limit 7 orig,ixg,ds,dv2,lv2 >> "$out"
  done
done
