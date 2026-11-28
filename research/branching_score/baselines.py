"""Reference policies on the test and validation splits: exact MRV, ungated ix (L1), the gated
final heuristic of the PR, and the double-precision L5 / LV fits.  usage: python3 baselines.py"""
import json
import math
import os
import subprocess

import fit2

HERE = fit2.HERE
L5 = {"wR": 0.05, "f3": 0.0, "f4": 1.0, "wprod": 4.0, "a": 0.5, "wsm": 0.5}
LVD = {"mode": 1, "g3": -0.25, "g4": 0.0, "al": 1.0, "be": 0.5, "gR": 0.005, "eps": 0.001}


def run_cmd(make, split):
    procs = {ds: subprocess.Popen(make(os.path.join(HERE, "sets", split, ds + ".txt")), stdout=subprocess.PIPE,
                                  text=True) for ds in fit2.SETS}
    out = {}
    for ds, pr in procs.items():
        tok = pr.communicate()[0].split()
        out[ds] = (float(tok[5]), float(tok[3]), int(tok[tok.index("capped") + 1]) if "capped" in tok else 0)
    return out


def p2(params):
    return lambda f: [os.path.join(HERE, "policy2"), f, "100000", "cap=%d" % fit2.CAP] + fit2.args_of(params)


POLICIES = [("L0 exact MRV", p2("mrv")), ("ix ungated (L1)", p2({})),
            ("ix gated final (PR)", lambda f: [os.path.join(HERE, "final_gated"), f, "100000"]),
            ("L5 (double)", p2(L5)), ("LV (double)", p2(LVD))]

if __name__ == "__main__":
    res = {}
    lines = []
    for split in ("test", "val"):
        base = run_cmd(POLICIES[0][1], split)
        lines.append("%s split, geomean(guesses+1) relative to exact MRV" % split)
        lines.append(fit2.header())
        for name, make in POLICIES:
            r = base if name.startswith("L0") else run_cmd(make, split)
            lines.append(fit2.row(name, r, base))
            res.setdefault(name, {})[split] = {"sets": r, "mean": math.exp(fit2.objective(r, base))}
        lines.append("")
    open(os.path.join(HERE, "baselines.txt"), "w").write("\n".join(lines) + "\n")
    json.dump(res, open(os.path.join(HERE, "baselines.json"), "w"), indent=1)
    print("\n".join(lines))
