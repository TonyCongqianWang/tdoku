#!/usr/bin/env python3
"""Summarize run_benchmark -c1 output blocks written by the pristine-vs-new comparison.
Handles locales with a decimal comma. Prints median usec/puzzle and ratio to pristine tdoku."""
import sys, collections, statistics
res = collections.defaultdict(list); cur = None
for l in open(sys.argv[1]):
    l = l.strip()
    if l.startswith('##'):
        cur = dict(x.split('=') for x in l[3:].split()); continue
    p = l.split(',')
    if len(p) == 13:  # decimal commas: pps, usec, %noguess, guesses each split in two
        usec = float(p[7] + '.' + p[8]); guesses = float(p[11] + '.' + p[12])
    else:
        usec = float(p[6]); guesses = float(p[8])
    res[(cur['file'], cur['build'] + ':' + p[4])].append((usec, guesses))
for f in sorted({k[0] for k in res}):
    base = statistics.median(u for u, _ in res[(f, 'pristine:tdoku')])
    out = []
    for k, v in res.items():
        if k[0] != f: continue
        m = statistics.median(u for u, _ in v)
        out.append(f'{k[1]}={m:.3f}us x{m / base:.3f} (g={v[0][1]:.2f}, n={len(v)})')
    print(f'{f}: ' + '; '.join(out))
