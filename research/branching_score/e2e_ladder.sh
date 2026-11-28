#!/bin/bash
# End-to-end guess counts (limit 2) of the ladder's fitted policies on the dev sets.
# usage: e2e_ladder.sh <out_file> <jobs>
cd "$(dirname "$0")"
out=$1; jobs=${2:-10}; : > "$out"
R=../data/dev; P=../dsel_study/dsel/data
SETS="hard:$R/hard.txt:1000 hard11:$R/hard11.txt:1000 hard1106:$R/hard1106.txt:157 magictour:$R/magictour.txt:571 17clue:$R/17clue.txt:2000 unbiased:$R/unbiased.txt:2000 pm:$P/pm.txt:200 pm_easy:$P/pm_easy.txt:200"
POL=("L0|mrv" "L1|" "L2|wR=0.02" "L3|wR=0.005 f3=0 f4=1" "L4|wR=0.05 f3=0 f4=0.5 wprod=4 a=0.5"
     "L5|wR=0.005 f3=0.5 f4=1 wprod=4 a=0.5 wsm=0.1 wsp=0.5")
{
  for s in $SETS; do
    IFS=: read name file n <<< "$s"
    echo "final_gated|$name|$file|$n|"
    for pol in "${POL[@]}"; do echo "${pol%%|*}|$name|$file|$n|${pol#*|}"; done
  done
} | xargs -P "$jobs" -d '\n' -I{} sh -c '
  IFS="|" read lvl name file n args <<EOT
{}
EOT
  if [ "$lvl" = final_gated ]; then r=$(./final_gated "$file" "$n"); else r=$(./policy_eval "$file" "$n" cap=5000000 $args); fi
  echo "$lvl $name $r" >> '"$out"
sort -o "$out" "$out"
