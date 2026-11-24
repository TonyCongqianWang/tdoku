#!/usr/bin/env python3
"""Trade-off of 'close the scoring gate if <rule>' rules on pooled decision records.
Good = closing nodes with refutation-free subtrees (multi-solution sets).
Bad  = closing nodes whose subtree has refutations, weighted by the number of refutations
       (proxy for what better branching there could have saved)."""
import csv, sys, os
F = ['depth', 'dec_before', 'fails_before', 'sols_before', 'total', 'unfixed', 'n2pairs',
     'subtree', 'refutations', 'fails_since_sol', 'n_cand', 'smin', 'smax', 'smean']
d = 'research/results/dev/decisions2'
UNIQUE = ['hard', 'hard11', 'magictour', '17clue', 'unbiased']
MULTI = ['serg', 'hard_minus1', 'hard_minus3', 'unbiased_minus1', 'unbiased_minus3', 'gen']
data = {n: [dict(zip(F, map(int, r[4:]))) for r in csv.reader(open(f'{d}/{n}.csv'))]
        for n in UNIQUE + MULTI}

def quant(xs, qs): xs = sorted(xs); return [xs[int(q * (len(xs) - 1))] for q in qs]
print('distributions (p10/p50/p90) of signals at decisions:')
for s in ['total', 'smean', 'smax', 'smin', 'depth']:
    print(f'  {s:6s} ' + '  '.join(f"{n}={'/'.join(map(str, quant([r[s] for r in data[n]], [.1, .5, .9])))}"
                                    for n in ['hard', 'magictour', '17clue', 'unbiased', 'serg', 'hard_minus3', 'unbiased_minus1']))

def evaluate(name, rule):
    out = []
    for n in UNIQUE + MULTI:
        rs = data[n]
        closed = [r for r in rs if rule(r)]
        ref_all = sum(r['refutations'] for r in rs) or 1
        rfree = [r for r in rs if r['refutations'] == 0]
        out.append((n, len(closed) / len(rs), sum(r['refutations'] for r in closed) / ref_all,
                    (sum(1 for r in rfree if rule(r)) / len(rfree)) if rfree else float('nan')))
    print(f'\n{name}')
    print('   ' + '  '.join(f'{n}: closed {c:.2f} refut-w {w:.3f} Rfree-closed {f:.2f}' if n in MULTI
                         else f'{n}: closed {c:.2f} refut-w {w:.3f}' for n, c, w, f in out))

for t in (90, 100, 110):
    evaluate(f'close if total < {t}', lambda r, t=t: r['total'] < t)
for t in (250, 270, 290):
    evaluate(f'close if smean >= {t} (x60 units, max 360)', lambda r, t=t: r['smean'] >= t)
for t in (3, 5, 8):
    evaluate(f'close if depth >= {t}', lambda r, t=t: r['depth'] >= t)
