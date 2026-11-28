#!/bin/bash
# Static cost of the choosers: llvm-mca (Alder Lake P-core model) on each noinline wrapper.
# Reports instruction count, single-call cycles (-iterations=1: latency of one cold call, no
# overlap) and block reciprocal throughput, over the LLVM-MCA-BEGIN/END region around the chooser
# (cold code after the function's ret, such as static-table initialisation, is excluded).
set -e
cd "$(dirname "$0")"
g++ -O3 -march=native -std=c++17 -ffp-contract=off -fno-asynchronous-unwind-tables -fpeel-loops --param max-completely-peeled-insns=4000 --param max-completely-peel-times=16 --param max-peeled-insns=4000 -I../../src -I../../include \
    -include cstdint -include cstring -S mca_choosers.cc -o mca/choosers.s
for k in "$@"; do
    awk -v f="$k" '$0 ~ "^"f":" {on=1; next} on && /^\t\.size/ {on=0} on' mca/choosers.s |
        grep -v -E '^\s*\.(cfi|p2align|loc|file)' > mca/$k.s
    ins=$(awk '/LLVM-MCA-BEGIN/ {on=1; next} /LLVM-MCA-END/ {on=0} on' mca/$k.s | grep -c -E '^\s+[a-z]')
    labels=$(grep -c -E '^\.L[0-9]+:' mca/$k.s || true)
    c1=$(llvm-mca-18 -mcpu=alderlake -iterations=1 mca/$k.s 2>/dev/null | awk '/Total Cycles/ {print $3}')
    rt=$(llvm-mca-18 -mcpu=alderlake -iterations=100 mca/$k.s 2>/dev/null | awk '/Block RThroughput/ {print $3}')
    be=$(awk '/LLVM-MCA-BEGIN/ {on=1} /LLVM-MCA-END/ {on=0} /^\.L[0-9]+:/ {seen[$1]=NR} on && /^[ \t]+j[a-z]+[ \t]+\.L/ {if (($2":") in seen) n++} END {print n+0}' mca/$k.s)
    [ "$be" != 0 ] && echo "WARNING: $k has $be loop back-edges in its region (counts are per loop body)"
    printf "%-8s instructions %5d  labels %3d  cycles(1 call) %6s  RThroughput %6s\n" "$k" "$ins" "$labels" "$c1" "$rt"
done
