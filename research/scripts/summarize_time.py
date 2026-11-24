#!/usr/bin/env python3
"""Pivot of paired time ratios (median over repetitions) from run_time.sh output, plus the
geometric mean across categories. usage: summarize_time.py file [file...]"""
import sys, math, os
from collections import defaultdict
ratio = defaultdict(dict); ns = defaultdict(dict); order = []
for path in sys.argv[1:]:
    for line in open(path):
        p = line.strip().split(',')
        if p[0] != 'time': continue
        cat = os.path.basename(p[2]).replace('.txt', ''); v = p[3]
        kv = dict(x.split('=') for x in p[5:] if '=' in x)
        ratio[v][cat] = float(kv['ratio_med']); ns[v][cat] = float(kv['median_ns'])
        if v not in order: order.append(v)
cats = sorted({c for v in ratio for c in ratio[v]})
print(f'{"variant":12s} ' + ' '.join(f'{c[:9]:>9s}' for c in cats) + '   geomean     worst')
for v in order:
    rs = [ratio[v].get(c, float('nan')) for c in cats]
    g = math.exp(sum(math.log(r) for r in rs) / len(rs))
    print(f'{v:12s} ' + ' '.join(f'{r:9.4f}' for r in rs) + f'   {g:7.4f}   {max(rs):7.4f}')
base = order[0]
print(f'\n{base} median ns/puzzle: ' + ', '.join(f'{c}={ns[base][c]:.0f}' for c in cats))
