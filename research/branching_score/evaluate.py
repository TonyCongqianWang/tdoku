"""Offline evaluation and fitting of branching scores on oracle data (see SPEC.md).

usage: python3 evaluate.py <oracle csv files or directories> ...

A policy maps a candidate to a score (lower is better); ties go to the first candidate in
(band, digit, config) order, as in the solver. Per node the chosen candidate's cost is compared
with the best candidate's (regret) and with exact MRV's. Nodes are weighted equally within a
puzzle, puzzles equally within a dataset, datasets equally overall.
"""
import csv
import glob
import math
import os
import sys
from collections import defaultdict, namedtuple

INF = float("inf")

# derived per-candidate features
F = namedtuple("F", "b d c n R m has2 is_mrv is_orig is_ix "
                    "xp xm xp_prod xm_prod xp_min xm_min xp_forced "
                    "sp_sum sp_min sp_zero sm_n sm_sum sm_zero depth cost")


def derive(c):
    rp = [c["kp%d" % j] / c["np%d" % j] for j in range(3)]
    rm = [c["km%d" % j] / c["nm%d" % j] for j in range(3)]
    sp = [c["sp%d" % j] for j in range(3)]
    sm = [c["sm%d" % j] for j in range(3) if c["sm%d" % j] != -9]
    return F(c["b"], c["d"], c["c"], c["n"], c["R"], c["m"], c["has2"], c["is_mrv"],
             c["is_orig"], c["is_ix"],
             sum(rp), sum(rm), rp[0] * rp[1] * rp[2], rm[0] * rm[1] * rm[2], min(rp), min(rm),
             sum(1 for j in range(3) if c["kp%d" % j] == 1 and c["np%d" % j] > 1),
             sum(sp), min(sp), sum(1 for s in sp if s == 0),
             len(sm), sum(sm), sum(1 for s in sm if s == 0), c["depth"], c["cost"])


def load(paths):
    """dataset -> puzzle -> list of nodes (each a list of F)"""
    files = []
    for p in paths:
        files += sorted(glob.glob(os.path.join(p, "*.csv"))) if os.path.isdir(p) else [p]
    data = defaultdict(lambda: defaultdict(dict))
    for path in files:
        with open(path) as f:
            r = csv.reader(f)
            header = next(r, None)
            if header is None:
                continue
            for row in r:
                if not row or row[0] == "dataset":
                    continue
                c = {k: (v if k == "dataset" else int(v)) for k, v in zip(header, row)}
                data[c["dataset"]][c["puzzle"]].setdefault(c["node"], []).append(derive(c))
    return {ds: {pz: list(nd.values()) for pz, nd in pzs.items()} for ds, pzs in data.items()}


def choose(cands, score):
    best, best_s = None, INF
    for c in cands:
        s = score(c)
        if s < best_s:
            best, best_s = c, s
    if best is None:  # no admissible candidate: exact MRV
        best = next(c for c in cands if c.is_mrv)
    return best


def evaluate(data, score):
    """per dataset: (mean log regret vs best, mean log ratio vs MRV, top-1 rate, sum ratio vs MRV)"""
    out = {}
    for ds, pzs in data.items():
        reg = rel = top = 0.0
        s_ch = s_mrv = 0
        for nodes in pzs.values():
            r = q = t = 0.0
            for cands in nodes:
                best = min(c.cost for c in cands)
                mrv = next(c.cost for c in cands if c.is_mrv)
                ch = choose(cands, score).cost
                r += math.log(ch / best)
                q += math.log(ch / mrv)
                t += ch == best
                s_ch += ch
                s_mrv += mrv
            reg += r / len(nodes)
            rel += q / len(nodes)
            top += t / len(nodes)
        n = len(pzs)
        out[ds] = (reg / n, rel / n, top / n, s_ch / s_mrv)
    return out


def objective(res, datasets=None):
    """mean over datasets of the mean log ratio vs MRV (lower is better)"""
    ds = datasets or list(res)
    return sum(res[d][1] for d in ds) / len(ds)


def report(rows, datasets):
    print("geo cost ratio vs MRV per node [top-1 rate]; last column: mean over datasets")
    print("%-34s" % "policy" + "".join("%15s" % d[:14] for d in datasets) + "%9s" % "mean")
    for name, res in rows:
        line = "%-34s" % name[:34]
        for d in datasets:
            line += "%9.3f [%3.0f]" % (math.exp(res[d][1]), 100 * res[d][2])
        line += "%9.3f" % math.exp(objective(res, datasets))
        print(line)
    sys.stdout.flush()


# ---------------------------------------------------------------------------------------------
# score families (parameters p are a dict)

def s_mrv(c):
    return 0 if c.is_mrv else INF


def s_orig(c):
    return 0 if c.is_orig else INF


def s_oracle_best(cands_cost):
    return cands_cost


def s_ix(c):
    """final heuristic, ungated: n = 2 only, summed crossing survival of both branches"""
    return c.xp + c.xm if c.n == 2 else INF


def make_score(p):
    """the general score at increasing complexity; absent parameters are 0 / disabled"""
    wR = p.get("wR", 0.0)
    f3 = p.get("f3", INF)       # penalty for n = 3 candidates (INF = filter)
    f4 = p.get("f4", INF)       # penalty for n >= 4
    a = p.get("a", 1.0)         # weight of the + branch crossing term for n >= 3
    bm = p.get("bm", 1.0)       # weight of the - branch crossing term for n >= 3
    wprod = p.get("wprod", 0.0)  # weight of the product aggregation (both branches)
    wsp = p.get("wsp", 0.0)     # same-band: weight of zero-slack mini-rows in the + branch
    wsm = p.get("wsm", 0.0)     # same-band: weight of zero-slack vacated mini-rows in - branch
    wnob = p.get("wnob", 0.0)   # band without an n = 2 digit (cross term with R)

    def score(c):
        if c.n == 2:
            s = c.xp + c.xm
        elif c.n == 3:
            if f3 == INF:
                return INF
            s = f3 + a * c.xp + bm * c.xm
        else:
            if f4 == INF:
                return INF
            s = f4 + a * c.xp + bm * c.xm
        s += wR * c.R + wprod * (c.xp_prod + c.xm_prod)
        s -= wsp * c.sp_zero + wsm * c.sm_zero
        s += wnob * (1 - c.has2) * c.R
        return s
    return score


def fit(data, p0, grid, datasets=None, sweeps=2):
    """coordinate search over parameter grids on the objective"""
    p = dict(p0)
    best = objective(evaluate(data, make_score(p)), datasets)
    for _ in range(sweeps):
        improved = False
        for k, values in grid.items():
            for v in values:
                q = dict(p)
                q[k] = v
                o = objective(evaluate(data, make_score(q)), datasets)
                if o < best - 1e-9:
                    best, p, improved = o, q, True
        if not improved:
            break
    return p, best


if __name__ == "__main__":
    data = load(sys.argv[1:])
    datasets = sorted(data)
    for ds in datasets:
        print("%-10s puzzles %4d nodes %5d candidates %7d" % (
            ds, len(data[ds]), sum(len(n) for n in data[ds].values()),
            sum(len(c) for n in data[ds].values() for c in n)))
    rows = [("exact MRV", evaluate(data, s_mrv)),
            ("orig (approx. MRV)", evaluate(data, s_orig)),
            ("ix ungated (final)", evaluate(data, s_ix)),
            ("oracle best", evaluate(data, lambda c: c.cost))]
    report(rows, datasets)
