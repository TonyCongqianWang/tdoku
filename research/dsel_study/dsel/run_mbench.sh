#!/bin/bash
# usage: run_mbench.sh <cpu> <out>
cpu=$1; out=$2; : > $out
for rep in 1 2 3; do
  for src in states/hard_s2048.bin states/17clue_s2048.bin states/unbiased_s2048.bin states/gen_s2048.bin states/serg_s2048.bin states/pm_s2048.bin synth:uniform synth:high; do
    for v in identity A B C D1 D2 E1 E2 F DB; do
      for m in tput lat; do taskset -c $cpu ./mbench $v $m $src 2048 1000 >> $out; done
    done
  done
done
