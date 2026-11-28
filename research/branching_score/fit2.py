"""End-to-end coordinate search with policy2 (cross terms, extended grids).

Objective: mean over the 8 dev sets of log(geomean(g+1) / exact MRV), on sets/train; every
result is also reported on sets/test. Puzzles are capped at CAP guesses.

usage: python3 fit2.py <experiment> [out_prefix]
  experiments:
    screen   from the fitted L5 point, a 1-D search over each cross term alone
    joint    coordinate search over the extended base grids plus every cross term
    cheap    restricted families (no union for the - branch, products only, ...) for latency
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
L5 = {"wR": 0.05, "f3": 0.0, "f4": 1.0, "wprod": 4.0, "a": 0.5, "wsm": 0.5}
L4 = {"wR": 0.05, "f3": 0.0, "f4": 1.0, "wprod": 4.0, "a": 0.5}

BASE_GRIDS = {
    "wR": [0.0, 0.02, 0.05, 0.1, 0.2],
    "f3": [-0.5, -0.25, 0.0, 0.25, 0.5, 1.0],
    "f4": [0.25, 0.5, 1.0, 1.5, 2.0, 3.0, INF],
    "a": [0.0, 0.1, 0.25, 0.5, 0.75, 1.0],
    "bm": [0.0, 0.25, 0.5, 0.75, 1.0, 1.5],
    "wprod": [2.0, 4.0, 6.0, 8.0, 12.0, 16.0, 24.0],
    "wsm": [0.0, 0.25, 0.5, 1.0],
    "wsp": [0.0, 0.25, 0.5],
}
CROSS_GRIDS = {
    "a2": [0.0, 0.25, 0.5, 0.75, 1.0, 1.5],
    "b2": [0.0, 0.25, 0.5, 0.75, 1.0, 1.5],
    "wpp": [-4.0, -2.0, -1.0, 1.0, 2.0, 4.0, 8.0],
    "wpm": [-4.0, -2.0, -1.0, 1.0, 2.0, 4.0, 8.0],
    "wbal": [-8.0, -4.0, -2.0, 2.0, 4.0, 8.0, 16.0],
    "wRp": [-0.2, -0.1, -0.05, 0.05, 0.1, 0.2],
    "wnp": [-2.0, -1.0, -0.5, 0.5, 1.0, 2.0, 4.0],
    "wmax": [-4.0, -2.0, 2.0, 4.0, 8.0],
    "wown": [-4.0, -2.0, 2.0, 4.0, 8.0],
    "wv": [-2.0, -1.0, 1.0, 2.0, 4.0, 8.0],
    "wlv": [-1.0, -0.5, -0.25, 0.25, 0.5, 1.0, 2.0],
    "wnm": [-0.5, -0.25, 0.25, 0.5, 1.0],
    "wsq": [-2.0, -1.0, -0.5, 0.5, 1.0, 2.0],
}


def args_of(p):
    if p == "mrv":
        return ["mrv"]
    return ["%s=%s" % (k, "inf" if v == INF else repr(v)) for k, v in sorted(p.items())]


def run(p, split, cap=CAP):
    procs = {}
    for ds in SETS:
        f = os.path.join(HERE, "sets", split, ds + ".txt")
        procs[ds] = subprocess.Popen([os.path.join(HERE, "policy2"), f, "100000", "cap=%d" % cap]
                                     + args_of(p), stdout=subprocess.PIPE, text=True)
    out = {}
    for ds, pr in procs.items():
        tok = pr.communicate()[0].split()
        out[ds] = (float(tok[5]), float(tok[3]), int(tok[tok.index("capped") + 1]))
    return out


def objective(res, base):
    return sum(math.log(res[d][0] / base[d][0]) for d in SETS) / len(SETS)


def row(name, res, base):
    line = "%-34s" % name[:34]
    for d in SETS:
        line += "%8.3f%s" % (res[d][0] / base[d][0], "*" if res[d][2] else " ")
    return line + "%8.3f" % math.exp(objective(res, base))


def header():
    return "%-34s" % "policy" + "".join("%9s" % d[:8] for d in SETS) + "%8s" % "mean"


class Fitter:
    def __init__(self):
        self.base_tr, self.base_te = run("mrv", "train"), run("mrv", "test")
        self.cache = {}

    def obj(self, q):
        key = tuple(sorted(q.items()))
        if key not in self.cache:
            self.cache[key] = objective(run(q, "train"), self.base_tr)
        return self.cache[key]

    def search(self, p, grids, passes=3):
        best = self.obj(p)
        for _ in range(passes):
            improved = False
            for k, values in grids.items():
                for v in values:
                    q = dict(p)
                    q[k] = v
                    if v == 0 and k in CROSS_GRIDS and k not in ("a2", "b2") and k not in p:
                        continue
                    o = self.obj(q)
                    if o < best - 1e-9:
                        best, p, improved = o, q, True
            if not improved:
                break
        return p, best

    def test(self, p):
        return run(p, "test")


def clean(p):
    return {k: v for k, v in p.items() if not (v == 0 and k in CROSS_GRIDS and k not in ("a2", "b2"))}


def main():
    exp = sys.argv[1]
    prefix = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "fit2_" + exp)
    F = Fitter()
    results = {}
    log = open(prefix + ".txt", "w")

    def emit(s):
        print(s)
        log.write(s + "\n")
        log.flush()
        sys.stdout.flush()

    emit("geomean(guesses+1) relative to exact MRV, test split (* = some puzzle capped at %d)" % CAP)
    emit(header())
    emit(row("L0 exact MRV", F.base_te, F.base_te))

    def record(name, p, tr):
        te = F.test(p)
        results[name] = {"params": {k: (None if v == INF else v) for k, v in p.items()},
                         "train": math.exp(tr), "test": math.exp(objective(te, F.base_te)),
                         "test_sets": te}
        emit(row(name, te, F.base_te) + "   train %.3f  %s" % (math.exp(tr), clean(p)))
        with open(prefix + ".json", "w") as f:
            json.dump(results, f, indent=1)

    t0 = time.time()
    if exp == "screen":
        record("L5", L5, F.obj(L5))
        for k, grid in CROSS_GRIDS.items():
            p, o = F.search(dict(L5), {k: grid}, passes=1)
            record("L5 + " + k, p, o)
    elif exp == "joint":
        p, o = F.search(dict(L5), BASE_GRIDS)
        record("L5 refit (extended grids)", p, o)
        grids = dict(BASE_GRIDS)
        grids.update(CROSS_GRIDS)
        p, o = F.search(p, grids)
        record("L5 + all cross terms", p, o)
    elif exp == "cheap":
        fams = [
            # products only: no sums, n = 2 included through a2 = b2 = 0
            ("prod only", {"wR": 0.05, "f3": 0.0, "f4": 1.0, "wprod": 4.0, "a": 0.0, "bm": 0.0,
                           "a2": 0.0, "b2": 0.0},
             {k: BASE_GRIDS[k] for k in ("wR", "f3", "f4", "wprod")}),
            # no - branch union: n >= 3 scored with + products and the digit volume only
            ("no union (wv)", {"wR": 0.05, "f3": 0.0, "f4": 1.0, "wprod": 0.0, "wpp": 4.0, "a": 0.5,
                               "bm": 0.0, "wv": 1.0},
             {"wR": BASE_GRIDS["wR"], "f3": BASE_GRIDS["f3"], "f4": BASE_GRIDS["f4"],
              "a": BASE_GRIDS["a"], "wpp": [1.0, 2.0, 4.0, 8.0, 16.0], "wv": [0.0, 0.5, 1.0, 2.0, 4.0, 8.0],
              "wlv": [0.0, 0.25, 0.5, 1.0]}),
            # n <= 3 only
            ("L4, n <= 3", dict(L4, f4=INF),
             {k: BASE_GRIDS[k] for k in ("wR", "f3", "a", "bm", "wprod")}),
        ]
        for name, start, grids in fams:
            p, o = F.search(start, grids)
            record(name, p, o)
    elif exp == "log":
        grids = {"al": [0.5, 0.75, 1.0, 1.25, 1.5, 2.0], "be": [0.0, 0.25, 0.5, 1.0, 2.0],
                 "g3": [-0.5, -0.25, 0.0, 0.25, 0.5, 1.0], "g4": [0.0, 0.25, 0.5, 1.0, 2.0, INF],
                 "gR": [0.0, 0.005, 0.01, 0.02, 0.05], "eps": [1e-4, 1e-3, 3e-3, 1e-2, 3e-2, 0.1]}
        p, o = F.search({"mode": 1, "g3": 0.0, "g4": 0.5}, grids)
        record("LV log(eps + P+ + P-)", p, o)
        g2 = {k: v for k, v in grids.items() if k != "be"}
        g2["cpick"] = [0, 1, 2]
        p, o = F.search({"mode": 2, "g3": 0.0, "g4": 0.5}, g2)
        record("DV log(eps + sum_c P+(c))", p, o)
    elif exp == "unionfree":
        g = {"g3": [-1.0, -0.5, -0.25, 0.0, 0.25, 0.5, 1.0], "g4": [-0.5, 0.0, 0.25, 0.5, 1.0, 2.0, INF],
             "gR": [0.0, 0.005, 0.01, 0.02, 0.05, 0.1], "sn": [0.0, 0.5, 1.0]}
        # DS: ix generalized to n >= 3 (sum over the digit's configurations of sum_j rho)
        p, o = F.search({"mode": 2, "wl": 0.0, "ws": 1.0, "g3": 0.0, "g4": 1.0}, g)
        record("DS sum_c sum_j rho", p, o)
        # DV: log digit volume
        gv = dict(g, al=[0.5, 0.75, 1.0, 1.5, 2.0], eps=[1e-4, 1e-3, 1e-2, 3e-2, 0.1])
        p, o = F.search({"mode": 2, "g3": 0.0, "g4": 0.5}, gv)
        record("DV log(eps + sum_c prod_j rho)", p, o)
        # DSV: sum + linear volume (the union-free analogue of L4)
        gl = dict(g, ws=[0.25, 0.5, 1.0], wlin=[1.0, 2.0, 4.0, 8.0, 16.0])
        p, o = F.search({"mode": 2, "wl": 0.0, "ws": 0.5, "wlin": 4.0, "g3": 0.0, "g4": 1.0}, gl)
        record("DSV ws*sum + wlin*vol", p, o)
        # DSLV: sum + log volume
        gm = dict(g, ws=[0.0, 0.1, 0.25, 0.5, 1.0], wl=[0.25, 0.5, 1.0, 2.0], eps=[1e-3, 1e-2, 3e-2, 0.1])
        p, o = F.search({"mode": 2, "wl": 1.0, "ws": 0.25, "g3": 0.0, "g4": 0.5}, gm)
        record("DSLV ws*sum + wl*log vol", p, o)
    emit("(%.0fs, %d evaluations)" % (time.time() - t0, len(F.cache)))


if __name__ == "__main__":
    main()
