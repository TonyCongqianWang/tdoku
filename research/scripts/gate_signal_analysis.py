#!/usr/bin/env python3
"""How well do cheap state signals predict that the subtree below a decision is refutation-free
(i.e. scoring there cannot shrink anything)?  Input: research_bench_stats -D files (extended)."""
import csv, sys, os, math

F = ['depth', 'dec_before', 'fails_before', 'sols_before', 'total', 'unfixed', 'n2pairs',
     'subtree', 'refutations', 'fails_since_sol', 'n_cand', 'smin', 'smax', 'smean']
SIGNALS = ['depth', 'total', 'n_cand', 'smin', 'smax', 'smean', 'fails_since_sol', 'sols_before']

def ranks(xs):
    o = sorted(range(len(xs)), key=lambda i: xs[i]); r = [0.0] * len(xs); i = 0
    while i < len(xs):
        j = i
        while j + 1 < len(xs) and xs[o[j + 1]] == xs[o[i]]: j += 1
        for k in range(i, j + 1): r[o[k]] = (i + j) / 2
        i = j + 1
    return r

def auc(scores, labels):
    r = ranks(scores); npos = sum(labels); nneg = len(labels) - npos
    if not npos or not nneg: return float('nan')
    return (sum(ri for ri, l in zip(r, labels) if l) - npos * (npos - 1) / 2) / (npos * nneg)

print(f"AUC of each signal for 'refutation-free subtree' (1.0/0.0 = perfect, 0.5 = useless;"
      f" <0.5 means smaller value -> refutation-free)")
print(f"{'set':16s} {'decisions':>9s} {'R-free':>6s} " + ' '.join(f'{s:>8s}' for s in SIGNALS))
for path in sys.argv[1:]:
    rows = [dict(zip(F, map(int, r[4:]))) for r in csv.reader(open(path))]
    lab = [1 if r['refutations'] == 0 else 0 for r in rows]
    name = os.path.basename(path)[:-4]
    print(f"{name:16s} {len(rows):9d} {sum(lab) / len(lab):6.3f} " +
          ' '.join(f"{auc([r[s] for r in rows], lab):8.3f}" for s in SIGNALS))
