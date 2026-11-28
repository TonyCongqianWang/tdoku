#!/bin/bash
# usage: run_e2e.sh <cpu> <out> <proc_id>
cpu=$1; out=$2; R=/home/twang/projects/repos/fun/tdoku/.claude/worktrees/tdoku-intersection-branching-9c3181/research/data/dev
cd "$(dirname "$0")"
run() { taskset -c $cpu ./bench_e2e "$@" >> $out; }
for group in tdoku ix; do
  run $group $R/kaggle.txt 2 2000 11 kaggle
  run $group $R/unbiased.txt 2 5000 11 unbiased
  run $group $R/17clue.txt 2 5000 11 17clue
  run $group $R/17clue.txt 1 5000 11 17clue
  run $group $R/magictour.txt 2 571 11 magictour
  run $group $R/hard.txt 2 1500 11 hard
  run $group $R/hard.txt 1 1500 11 hard
  run $group $R/hard11.txt 2 1000 11 hard11
  run $group $R/serg.txt 2 2000 11 serg
  run $group $R/gen.txt 2 10000 11 gen
  run $group data/pm_easy.txt 2 2000 11 pm_easy
  run $group data/pm_easy.txt 1 2000 11 pm_easy
  run $group data/pm_hard25.txt 2 25 11 pm_hard
  run $group data/pm_hard25.txt 1 25 11 pm_hard
done
