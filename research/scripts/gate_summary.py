#!/usr/bin/env python3
"""Geomean of paired time ratios over all cells of a gate comparison (t2 + t1000 files),
vs the original and vs the in-binary control. usage: gate_summary.py <dir>"""
import statistics, os, math, sys
from collections import defaultdict
S = sys.argv[1]
cells = defaultdict(dict)
for w in ['t2', 't1000']:
    r = defaultdict(lambda: defaultdict(list))
    for p in (1, 2, 3, 4, 5):
        fn = f'{S}/{w}.proc{p}.csv'
        if not os.path.exists(fn): continue
        for line in open(fn):
            q = line.strip().split(',')
            if q[0] != 'time': continue
            kv = dict(x.split('=') for x in q[5:] if '=' in x)
            r[q[3]][os.path.basename(q[2])[:-4]].append(float(kv['ratio_med']))
    for v in r:
        for c in r[v]: cells[v][w + ':' + c] = statistics.median(r[v][c])
g = lambda xs: math.exp(sum(map(math.log, xs)) / len(xs))
print(f"{'variant':10s} {'vs_orig':>8s} {'limit2':>7s} {'enum':>7s} {'vs_ctrl':>8s}  worst            serg(l2)")
for v in cells:
    c = cells[v]
    worst = max(c.items(), key=lambda kv: kv[1])
    t2 = [x for k, x in c.items() if k.startswith('t2:')]; t1 = [x for k, x in c.items() if k.startswith('t1000:')]
    vc = g([c[k] / cells['CTRL'][k] for k in c]) if 'CTRL' in cells else float('nan')
    serg = c.get('t2:serg', float('nan'))
    print(f"{v:10s} {g(list(c.values())):8.4f} {g(t2):7.4f} {g(t1):7.4f} {vc:8.4f}  {worst[1]:.3f} {worst[0]:18s} {serg:.3f}")
