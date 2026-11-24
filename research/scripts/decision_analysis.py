#!/usr/bin/env python3
"""Which cheap state features predict how much search lies below a decision?

Input: research_bench_stats -D output (tag,file,idx,variant,depth,decisions_before,fails_before,
solutions_before,total_configs,unfixed_bands,n2_pairs,subtree).
For each category: share of all decisions that sit in subtrees of size >= S, and for each
feature the Spearman correlation with log(subtree) plus how the total subtree mass splits
across feature thresholds (the quantity a gate controls: the fraction of decisions taken with
the expensive score vs. the fraction of 'search mass' covered)."""
import csv, sys, math, os
from collections import defaultdict

def ranks(xs):
    o = sorted(range(len(xs)), key=lambda i: xs[i]); r = [0.0] * len(xs); i = 0
    while i < len(xs):
        j = i
        while j + 1 < len(xs) and xs[o[j + 1]] == xs[o[i]]: j += 1
        for k in range(i, j + 1): r[o[k]] = (i + j) / 2
        i = j + 1
    return r

def spearman(a, b):
    ra, rb = ranks(a), ranks(b); n = len(a); ma = sum(ra) / n; mb = sum(rb) / n
    num = sum((x - ma) * (y - mb) for x, y in zip(ra, rb))
    den = math.sqrt(sum((x - ma) ** 2 for x in ra) * sum((y - mb) ** 2 for y in rb))
    return num / den if den else float('nan')

path, variant = sys.argv[1], sys.argv[2]
rows = defaultdict(list)
for r in csv.reader(open(path)):
    if r[3] != variant: continue
    rows[os.path.basename(r[1])[:-4]].append([int(x) for x in r[4:]])
F = ['depth', 'dec_before', 'fails_before', 'sols_before', 'total_cfg', 'unfixed', 'n2pairs']
for cat, rs in sorted(rows.items()):
    sub = [r[7] for r in rs]; ls = [math.log(s) for s in sub]
    print(f'== {cat}: decisions={len(rs)}  subtree==1: {sum(1 for s in sub if s == 1) / len(rs):.2f}'
          f'  mean subtree={sum(sub) / len(sub):.1f}')
    print('   spearman(log subtree): ' + '  '.join(
        f'{f}={spearman([r[k] for r in rs], ls):+.2f}' for k, f in enumerate(F)))
