#!/usr/bin/env python3
"""Summarize per-puzzle count CSVs from research_bench(-stats) -m count -o.

Row format: tag,file,idx,variant,guesses,solutions[,nodes,fails,select_cycles]

For each (file, variant) prints mean guesses per puzzle, the paired ratio of means against the
baseline variant with a 95% CI (delta method on paired differences), fraction of puzzles with
fewer / more guesses than the baseline, and (stats build) failed branches and selection cycles
per node. Optional: --pivot prints a variant x category matrix of ratios.
"""
import argparse, csv, math, os
from collections import defaultdict


def load(paths):
    data = defaultdict(lambda: defaultdict(dict))   # file -> variant -> idx -> row
    order = []
    for p in paths:
        for r in csv.reader(open(p)):
            f, idx, v = os.path.basename(r[1]), int(r[2]), r[3]
            vals = [float(x) for x in r[4:]]
            data[f][v][idx] = vals
            if v not in order:
                order.append(v)
    return data, order


def ratio_ci(base, var):
    n = len(base)
    mb, mv = sum(base) / n, sum(var) / n
    if mb == 0:
        return float('nan'), float('nan')
    R = mv / mb
    d = [v - R * b for b, v in zip(base, var)]
    md = sum(d) / n
    sd = math.sqrt(sum((x - md) ** 2 for x in d) / max(n - 1, 1))
    return R, 1.96 * sd / (math.sqrt(n) * mb)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('csv', nargs='+')
    ap.add_argument('--base', default='A_orig')
    ap.add_argument('--pivot', action='store_true')
    a = ap.parse_args()
    data, order = load(a.csv)
    piv = defaultdict(dict)
    for f in sorted(data):
        vs = data[f]
        if a.base not in vs:
            continue
        idxs = sorted(vs[a.base])
        base_g = [vs[a.base][i][0] for i in idxs]
        if not a.pivot:
            print(f'\n== {f}  (n={len(idxs)}, baseline {a.base}: '
                  f'{sum(base_g) / len(idxs):.3f} guesses/puzzle)')
            print(f'{"variant":22s} {"guess/pz":>9s} {"ratio":>7s} {"±95%":>6s} '
                  f'{"better":>7s} {"worse":>7s} {"fails/pz":>9s} {"selcyc/node":>11s}')
        for v in order:
            if v not in vs:
                continue
            g = [vs[v][i][0] for i in idxs]
            R, ci = ratio_ci(base_g, g)
            better = sum(1 for b, x in zip(base_g, g) if x < b) / len(idxs)
            worse = sum(1 for b, x in zip(base_g, g) if x > b) / len(idxs)
            row = vs[v][idxs[0]]
            fails = selc = float('nan')
            if len(row) >= 5:
                nodes = sum(vs[v][i][2] for i in idxs)
                fails = sum(vs[v][i][3] for i in idxs) / len(idxs)
                selc = sum(vs[v][i][4] for i in idxs) / max(nodes, 1)
            piv[v][f] = R
            if not a.pivot:
                print(f'{v:22s} {sum(g) / len(g):9.3f} {R:7.4f} {ci:6.4f} {better:7.3f} '
                      f'{worse:7.3f} {fails:9.3f} {selc:11.1f}')
    if a.pivot:
        files = sorted(data)
        short = [f.replace('.txt', '')[:9] for f in files]
        print(f'{"variant":22s} ' + ' '.join(f'{s:>9s}' for s in short))
        for v in order:
            print(f'{v:22s} ' + ' '.join(
                f'{piv[v].get(f, float("nan")):9.4f}' for f in files))


if __name__ == '__main__':
    main()
