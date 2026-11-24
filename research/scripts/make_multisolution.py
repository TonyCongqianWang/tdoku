#!/usr/bin/env python3
"""Derive multi-solution workloads by deleting k random clues from (unique) puzzles.
usage: make_multisolution.py <in> <out> <k> <count> <seed>"""
import random, sys
src, out, k, count, seed = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
rng = random.Random(seed)
ps = [l[:81] for l in open(src) if len(l) >= 81 and not l.startswith('#')][:count]
with open(out, 'w') as f:
    for p in ps:
        clues = [i for i, c in enumerate(p) if c not in '.0']
        q = list(p)
        for i in rng.sample(clues, k): q[i] = '.'
        f.write(''.join(q) + '\n')
