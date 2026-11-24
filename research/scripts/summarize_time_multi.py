#!/usr/bin/env python3
"""Median / min / max over independent processes of the per-process paired time ratio.
usage: summarize_time_multi.py out_prefix.proc*.csv"""
import sys, os, math, statistics
from collections import defaultdict
r = defaultdict(lambda: defaultdict(list)); order = []
for path in sys.argv[1:]:
    for line in open(path):
        p = line.strip().split(',')
        if p[0] != 'time': continue
        cat = os.path.basename(p[2])[:-4]; v = p[3]
        kv = dict(x.split('=') for x in p[5:] if '=' in x)
        r[v][cat].append(float(kv['ratio_med']))
        if v not in order: order.append(v)
cats = sorted({c for v in r for c in r[v]})
print(f'{"variant":8s} ' + ' '.join(f'{c[:9]:>17s}' for c in cats) + '   geomean(medians)')
for v in order:
    cells, meds = [], []
    for c in cats:
        xs = r[v][c]; m = statistics.median(xs); meds.append(m)
        cells.append(f'{m:.3f} [{min(xs):.3f},{max(xs):.3f}]')
    g = math.exp(sum(math.log(x) for x in meds) / len(meds))
    print(f'{v:8s} ' + ' '.join(f'{c:>17s}' for c in cells) + f'   {g:.4f}')
print(f'processes per cell: {len(r[order[0]][cats[0]])}')
