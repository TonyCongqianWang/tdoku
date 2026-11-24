#!/bin/bash
# Deterministic search-tree statistics (stats build) for a set of variant groups on a split.
#
#   research/scripts/run_counts.sh <split> <limit> <out_prefix> <transform_seed> <group>...
#
# Runs every category file of research/data/<split>/ in parallel (count mode is deterministic,
# so parallel execution does not affect results). Writes <out_prefix>.summary (aggregate lines)
# and <out_prefix>.csv (per puzzle).
set -euo pipefail
split=$1; limit=$2; out=$3; seed=$4; shift 4
root=$(cd "$(dirname "$0")/../.." && pwd)
spec=$(python3 "$root/research/scripts/variants.py" "$@")
mkdir -p "$(dirname "$out")"
rm -f "$out".csv "$out".summary "$out".part.*
ls "$root"/research/data/"$split"/*.txt | xargs -P 8 -I{} bash -c '
  f={}; b=$(basename $f .txt)
  '"$root"'/build/research_bench_stats -m count -l '"$limit"' -x '"$seed"' -t '"$split"' \
     -v "'"$spec"'" -o '"$out"'.part.$b.csv $f > '"$out"'.part.$b.summary'
cat "$out".part.*.csv > "$out".csv
cat "$out".part.*.summary > "$out".summary
rm -f "$out".part.*
grep ERROR "$out".summary || true
