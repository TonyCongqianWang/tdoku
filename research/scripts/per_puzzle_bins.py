#!/usr/bin/env python3
"""Per-puzzle decision ratios binned by baseline difficulty.
usage: per_puzzle_bins.py counts.csv base variant [category...]"""
import csv, sys, os, math
from collections import defaultdict
path, base, var = sys.argv[1:4]; cats = set(sys.argv[4:])
d = defaultdict(dict)
for r in csv.reader(open(path)):
    c = os.path.basename(r[1]).replace('.txt', '')
    if cats and c not in cats: continue
    d[(c, r[2])][r[3]] = int(r[4])
for c in sorted({k[0] for k in d}):
    pairs = [(v[base], v[var]) for k, v in d.items() if k[0] == c and base in v and var in v]
    pairs.sort()
    n = len(pairs)
    print(f'== {c} (n={n})  base_mean={sum(p[0] for p in pairs)/n:.2f} var_mean={sum(p[1] for p in pairs)/n:.2f}')
    print(f'   better={sum(1 for a,b in pairs if b<a)/n:.3f} equal={sum(1 for a,b in pairs if b==a)/n:.3f} worse={sum(1 for a,b in pairs if b>a)/n:.3f}'
          f'  geo((v+1)/(b+1))={math.exp(sum(math.log((b+1)/(a+1)) for a,b in pairs)/n):.3f}')
    for q in range(5):
        chunk = pairs[q*n//5:(q+1)*n//5]
        if not chunk: continue
        sa = sum(p[0] for p in chunk); sb = sum(p[1] for p in chunk)
        worse = sum(1 for a,b in chunk if b > 1.5*a + 2) / len(chunk)
        print(f'   baseline quintile {q+1}: base guesses {chunk[0][0]}-{chunk[-1][0]}, ratio of means '
              f'{sb/max(sa,1):.3f}, P(var > 1.5*base+2)={worse:.3f}')
