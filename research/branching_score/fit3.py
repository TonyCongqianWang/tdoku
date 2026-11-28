"""Refit the SIMD candidates (fixed-point implementations in policy_simd) end to end.

Same protocol as fit2.py: coordinate search on sets/train, objective = mean over the 8 dev sets of
log(geomean(g+1) / exact MRV), reported on sets/test.

usage: python3 fit3.py <ds|dv|lv> [wide]
  wide: finer grids and more passes; also reports the validation split (sets/val: unseen puzzles of
  the research val sets and the unused pencilmark puzzles)
"""
import json
import math
import os
import subprocess
import sys
import time

import fit2

HERE = fit2.HERE
START = {
    "ds": {"sn": 0.5, "g3": -0.25, "g4": -0.5, "gR": 0.02},
    "dv": {"al": 1.25, "eps": 1e-4, "g3": 0.0, "g4": 0.25, "gR": 0.02, "cpick": 1},
    "lv": {"al": 1.0, "be": 0.5, "eps": 1e-3, "g3": -0.25, "g4": 0.0, "gR": 0.005},
    "dv2": {"al": 1.1, "g3": -0.15, "g4": 0.1, "gR": 0.015, "cpick": 1},
    "lv2": {"al": 1.1, "be": 0.6, "g3": -0.25, "g4": -0.1, "gR": 0.015},
}
GRIDS = {
    "ds": {"sn": [0.25, 0.4, 0.5, 0.6, 0.75], "g3": [-0.5, -0.25, -0.1, 0.0, 0.25],
           "g4": [-1.0, -0.75, -0.5, -0.25, 0.0], "gR": [0.0, 0.01, 0.02, 0.03, 0.05]},
    "dv": {"al": [0.9, 1.0, 1.1, 1.25], "eps": [1e-4, 3e-4, 1e-3, 1e-2], "g3": [-0.25, -0.1, 0.0, 0.1, 0.25],
           "g4": [0.0, 0.1, 0.25, 0.5], "gR": [0.01, 0.02, 0.03, 0.05], "cpick": [0, 1]},
    "lv": {"al": [0.75, 0.9, 1.0, 1.1, 1.25], "be": [0.25, 0.5, 0.75, 1.0], "eps": [1e-4, 3e-4, 1e-3, 1e-2],
           "g3": [-0.5, -0.25, -0.1, 0.0], "g4": [-0.25, 0.0, 0.25], "gR": [0.0, 0.005, 0.01, 0.02]},
}


WIDE = {
    "ds": {"sn": [0.3, 0.4, 0.45, 0.5, 0.55, 0.6, 0.7], "g3": [-0.75, -0.5, -0.35, -0.25, -0.15, 0.0, 0.25],
           "g4": [-1.5, -1.0, -0.75, -0.5, -0.35, -0.25, 0.0], "gR": [0.0, 0.01, 0.015, 0.02, 0.025, 0.03, 0.04]},
    "dv": {"al": [0.9, 1.0, 1.05, 1.1, 1.15, 1.2, 1.25], "eps": [3e-5, 1e-4, 3e-4, 1e-3, 3e-3],
           "g3": [-0.5, -0.35, -0.25, -0.15, -0.1, 0.0, 0.1], "g4": [-0.25, -0.1, 0.0, 0.1, 0.25],
           "gR": [0.01, 0.015, 0.02, 0.025, 0.03, 0.04], "cpick": [0, 1]},
    "lv": {"al": [0.9, 1.0, 1.05, 1.1, 1.15, 1.2, 1.25], "be": [0.25, 0.4, 0.5, 0.6, 0.75, 1.0],
           "eps": [1e-4, 3e-4, 1e-3, 3e-3, 1e-2], "g3": [-0.5, -0.35, -0.25, -0.15, -0.1, 0.0],
           "g4": [-0.25, -0.1, 0.0, 0.1, 0.25], "gR": [0.0, 0.005, 0.0075, 0.01, 0.015, 0.02]},
    "dv2": {"al": [0.9, 1.0, 1.05, 1.1, 1.15, 1.2, 1.25], "g3": [-0.5, -0.35, -0.25, -0.15, -0.1, 0.0, 0.1],
            "g4": [-0.25, -0.1, 0.0, 0.1, 0.25, 0.5], "gR": [0.005, 0.01, 0.015, 0.02, 0.025, 0.03, 0.04],
            "cpick": [0, 1]},
    "lv2": {"al": [0.9, 1.0, 1.05, 1.1, 1.15, 1.2, 1.25], "be": [0.25, 0.4, 0.5, 0.6, 0.75, 1.0],
            "g3": [-0.5, -0.35, -0.25, -0.15, -0.1, 0.0], "g4": [-0.25, -0.1, 0.0, 0.1, 0.25],
            "gR": [0.0, 0.005, 0.01, 0.015, 0.02, 0.025, 0.03]},
}


def make_run(kind):
    def run(p, split, cap=fit2.CAP):
        procs = {}
        for ds in fit2.SETS:
            f = os.path.join(HERE, "sets", split, ds + ".txt")
            cmd = [os.path.join(HERE, "policy_simd_fit"), f, "100000", kind] if p != "mrv" else \
                  [os.path.join(HERE, "policy2"), f, "100000", "mrv"]
            if p != "mrv":
                cmd += ["%s=%r" % (k, v) for k, v in sorted(p.items())]
            procs[ds] = subprocess.Popen(cmd + ["cap=%d" % cap], stdout=subprocess.PIPE, text=True)
        out = {}
        for ds, pr in procs.items():
            tok = pr.communicate()[0].split()
            out[ds] = (float(tok[5]), float(tok[3]), int(tok[tok.index("capped") + 1]))
        return out
    return run


def main():
    kind = sys.argv[1]
    wide = len(sys.argv) > 2 and sys.argv[2] == "wide"
    run = fit2.run = make_run(kind)
    F = fit2.Fitter()
    prefix = os.path.join(HERE, "fit3_" + kind + ("_wide" if wide else ""))
    t0 = time.time()
    start = dict(START[kind])
    o0 = F.obj(start)
    p, o = F.search(start, WIDE[kind] if wide else GRIDS[kind], passes=4 if wide else 3)
    base_val = run("mrv", "val") if wide else None
    rows = []
    with open(prefix + ".txt", "w") as log:
        def emit(s):
            print(s)
            log.write(s + "\n")
        emit("SIMD %s, geomean(guesses+1) relative to exact MRV, test split (* = capped at %d)" % (kind, fit2.CAP))
        emit(fit2.header())
        emit(fit2.row("L0 exact MRV", F.base_te, F.base_te))
        res = {}
        for name, q, oo in [("start (double fit)", start, o0), ("refit (SIMD)", p, o)]:
            te = F.test(q)
            emit(fit2.row("%s %s" % (kind.upper(), name), te, F.base_te) + "   train %.3f  %s" % (math.exp(oo), q))
            res[name] = {"params": q, "train": math.exp(oo), "test": math.exp(fit2.objective(te, F.base_te)),
                         "test_sets": te}
            if wide:
                va = run(q, "val")
                emit(fit2.row("  validation", va, base_val))
                res[name]["val"] = math.exp(fit2.objective(va, base_val))
                res[name]["val_sets"] = va
        emit("(%.0fs, %d evaluations)" % (time.time() - t0, len(F.cache)))
    with open(prefix + ".json", "w") as f:
        json.dump(res, f, indent=1)


if __name__ == "__main__":
    main()
