"""Fit the complexity ladder directly on end-to-end guess counts (limit 2, no gate).

Coordinate search on sets/train (even puzzles of each dev set); each level is reported on
sets/test (odd puzzles). Objective: mean over datasets of log(geomean(g+1) / that of exact MRV).
Puzzles are capped at CAP guesses during fitting (the count is then a lower bound).

usage: python3 fit_e2e.py [out_json]
"""
import json
import math
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SETS = ["hard", "hard11", "hard1106", "magictour", "17clue", "unbiased", "pm", "pm_easy"]
CAP = 300000
INF = float("inf")


def args_of(p):
    if p == "mrv":
        return ["mrv"]
    return ["%s=%s" % (k, "inf" if v == INF else repr(v)) for k, v in sorted(p.items())]


def run(p, split, cap=CAP):
    """geomean(g+1), mean and capped count per dataset, datasets in parallel"""
    procs = {}
    for ds in SETS:
        f = os.path.join(HERE, "sets", split, ds + ".txt")
        procs[ds] = subprocess.Popen([os.path.join(HERE, "policy_eval"), f, "100000", "cap=%d" % cap]
                                     + args_of(p), stdout=subprocess.PIPE, text=True)
    out = {}
    for ds, pr in procs.items():
        tok = pr.communicate()[0].split()
        out[ds] = (float(tok[5]), float(tok[3]), int(tok[tok.index("capped") + 1]))
    return out


def objective(res, base):
    return sum(math.log(res[d][0] / base[d][0]) for d in SETS) / len(SETS)


def table(rows, base):
    print("geomean(guesses+1) relative to exact MRV (end to end, limit 2, no gate); [capped puzzles]")
    print("%-28s" % "policy" + "".join("%13s" % d for d in SETS) + "%9s" % "mean")
    for name, res in rows:
        line = "%-28s" % name[:28]
        for d in SETS:
            r = res[d][0] / base[d][0]
            line += "%9.3f%s" % (r, " [%d]" % res[d][2] if res[d][2] else "    ")
        print(line + "%9.3f" % math.exp(objective(res, base)))
    sys.stdout.flush()


LEVELS = [
    ("L1 final ungated", {}),
    ("L2 + w_R*R", {"wR": [0.0, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0]}),
    ("L3 + n>=3 candidates", {"f3": [INF, 0.0, 0.5, 1.0, 2.0, 3.0], "f4": [INF, 1.0, 2.0, 4.0],
                              "a": [0.5, 1.0, 2.0], "bm": [0.0, 0.5, 1.0, 2.0]}),
    ("L4 + product", {"wprod": [0.0, -1.0, 1.0, 2.0, 4.0]}),
    ("L5 + same-band slack", {"wsp": [0.0, 0.25, 0.5, 1.0], "wsm": [0.0, 0.25, 0.5, 1.0]}),
]

if __name__ == "__main__":
    out_json = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "fit_e2e.json")
    base_tr, base_te = run("mrv", "train"), run("mrv", "test")
    rows = [("L0 exact MRV", base_te)]
    p, cum, fitted = {}, {}, {}
    cache = {}

    def obj(q):
        key = tuple(sorted(q.items()))
        if key not in cache:
            cache[key] = objective(run(q, "train"), base_tr)
        return cache[key]

    for name, grid in LEVELS:
        t0 = time.time()
        cum.update(grid)
        best = obj(p)
        for _ in range(2):
            improved = False
            for k, values in cum.items():
                for v in values:
                    q = dict(p)
                    q[k] = v
                    o = obj(q)
                    if o < best - 1e-9:
                        best, p, improved = o, q, True
            if not improved:
                break
        res = run(p, "test")
        rows.append((name, res))
        fitted[name] = {"params": {k: (None if v == INF else v) for k, v in p.items()},
                        "train_obj": best, "test": res}
        print("\n%s  params %s  train %.3f  (%.0fs, %d evaluations so far)" % (
            name, p, math.exp(best), time.time() - t0, len(cache)))
        table(rows, base_te)
        with open(out_json, "w") as f:
            json.dump({"base_test": base_te, "levels": fitted}, f, indent=1)
