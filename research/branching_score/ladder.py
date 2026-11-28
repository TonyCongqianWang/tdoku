"""Complexity ladder: add one ingredient of the general score per level, fit it by coordinate
search on the even puzzles of every dev set, report on the odd (held-out) puzzles.

usage: python3 ladder.py <oracle data dir>
"""
import sys
import time

from evaluate import INF, evaluate, fit, load, make_score, objective, report, s_ix, s_mrv

data = load(sys.argv[1:])
datasets = sorted(data)
train = {ds: {pz: v for pz, v in pzs.items() if pz % 2 == 0} for ds, pzs in data.items()}
test = {ds: {pz: v for pz, v in pzs.items() if pz % 2 == 1} for ds, pzs in data.items()}
for ds in datasets:
    print("%-10s puzzles %4d nodes %5d candidates %7d" % (
        ds, len(data[ds]), sum(len(n) for n in data[ds].values()),
        sum(len(c) for n in data[ds].values() for c in n)))

LEVELS = [
    ("L1 final ungated (n=2, X+ + X-)", {}),
    ("L2 + band restriction w_R*R", {"wR": [0.0, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5]}),
    ("L3 + n>=3 candidates f_n, a*X+ + b*X-",
     {"f3": [INF, 0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0], "f4": [INF, 0.5, 1.0, 2.0, 3.0, 5.0],
      "a": [0.5, 1.0, 1.5, 2.0, 3.0], "bm": [0.0, 0.5, 1.0, 1.5, 2.0]}),
    ("L4 + product aggregation", {"wprod": [0.0, -2.0, -1.0, -0.5, 0.5, 1.0, 2.0, 4.0]}),
    ("L5 + same-band zero-slack rows", {"wsp": [0.0, 0.1, 0.25, 0.5, 1.0], "wsm": [0.0, 0.1, 0.25, 0.5, 1.0]}),
    ("L6 + cross term no-pair band * R", {"wnob": [0.0, -0.1, -0.05, -0.02, 0.02, 0.05, 0.1]}),
]

rows = [("L0 exact MRV", evaluate(test, s_mrv))]
report(rows, datasets)
p, cum = {}, {}
for name, grid in LEVELS:
    cum.update(grid)
    t0 = time.time()
    if grid:
        # the new ingredient enters at its neutral value; refit the new and the old parameters
        p, train_obj = fit(train, p, cum, sweeps=2)
    res = evaluate(test, make_score(p))
    rows.append((name, res))
    print("\n%s  params %s  (fit %.0fs)" % (name, {k: v for k, v in p.items()}, time.time() - t0))
    report(rows, datasets)
assert abs(objective(evaluate(test, s_ix)) - objective(rows[1][1])) < 1e-12
