"""Tail-sensitive offline metrics for the ladder's fitted policies (held-out odd puzzles)."""
import math, sys
from evaluate import INF, load, make_score, choose, s_mrv
data = load(["data/dev"])
test = {ds: {pz: v for pz, v in pzs.items() if pz % 2 == 1} for ds, pzs in data.items()}
P = [("L0 MRV", None), ("L1", {}), ("L2", {'wR': 0.02}), ("L3", {'wR': 0.005, 'f3': 0.0, 'f4': 1.0}),
     ("L4", {'wR': 0.05, 'f3': 0.0, 'f4': 0.5, 'wprod': 4.0, 'a': 0.5}),
     ("L5", {'wR': 0.005, 'f3': 0.5, 'f4': 1.0, 'wprod': 4.0, 'a': 0.5, 'wsm': 0.1, 'wsp': 0.5}),
     ("best", "best")]
ds_list = sorted(test)
print("sum(cost chosen)/sum(cost MRV) | share of nodes with chosen > 2x MRV (censored = budget)")
print("%-8s" % "" + "".join("%20s" % d for d in ds_list))
for name, p in P:
    sc = s_mrv if p is None else (lambda c: c.cost) if p == "best" else make_score(p)
    line = "%-8s" % name
    for ds in ds_list:
        sch = smrv = bad = nn = 0
        for nodes in test[ds].values():
            for cands in nodes:
                mrv = next(c.cost for c in cands if c.is_mrv)
                ch = choose(cands, sc).cost
                sch += ch; smrv += mrv; nn += 1; bad += ch > 2 * mrv
        line += "%12.3f %6.1f%%" % (sch / smrv, 100 * bad / nn)
    print(line)
