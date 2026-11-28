#!/bin/bash
# Gather oracle data for the dev sets in parallel chunks.  usage: gather.sh <out_dir> <jobs>
cd "$(dirname "$0")"
out=$1; jobs=${2:-8}; mkdir -p "$out"
R=../data/dev; P=../dsel_study/dsel/data
K=8
{
  for spec in hard:$R/hard.txt:400:50 17clue:$R/17clue.txt:400:100 unbiased:$R/unbiased.txt:400:100 \
              serg:$R/serg.txt:400:100 magictour:$R/magictour.txt:400:50 hard11:$R/hard11.txt:400:50 \
              hard1106:$R/hard1106.txt:157:20 pm:$P/pm.txt:300:15 pm_easy:$P/pm_easy.txt:300:15; do
    IFS=: read name file n chunk <<< "$spec"
    for ((s = 0; s < n; s += chunk)); do
      c=$chunk; (( s + c > n )) && c=$((n - s))
      echo "$name $file $s $c"
    done
  done
} | xargs -P "$jobs" -L 1 sh -c './oracle "$1" "$0" "$2" "$3" '$K' > '"$out"'/"$0"_"$2".csv'
