#!/usr/bin/env python3
"""Analyze per-decision traces from research_bench_stats -T.

Columns: tag,file,idx,variant,n,cfgmask,asserted,ok,total_before,total_after,
         ny0,ny1,ny2,nya0,nya1,nya2,K00,K01,K02,K10,K11,K12,K20,K21,K22

Reports, for the positive branch actually explored:
  * how often the intersection estimate is informative (some k_y < n_y)
  * how often the estimate alone already proves failure (some k_y == 0)
  * Spearman correlation of estimated vs. actual opposite-band eliminations (surviving branches)
  * Spearman correlation of estimated opposite-band eliminations vs. total eliminations
  * AUC of several estimates for predicting failure of the positive branch
"""
import csv, sys, math
from collections import defaultdict

ELEM = [(0,1,2),(1,2,0),(2,0,1),(2,1,0),(0,2,1),(1,0,2)]

def ranks(xs):
    order = sorted(range(len(xs)), key=lambda i: xs[i])
    r = [0.0]*len(xs); i = 0
    while i < len(xs):
        j = i
        while j+1 < len(xs) and xs[order[j+1]] == xs[order[i]]: j += 1
        for k in range(i, j+1): r[order[k]] = (i+j)/2.0
        i = j+1
    return r

def pearson(a, b):
    n = len(a); ma = sum(a)/n; mb = sum(b)/n
    sa = math.sqrt(sum((x-ma)**2 for x in a)); sb = math.sqrt(sum((y-mb)**2 for y in b))
    if sa == 0 or sb == 0: return float('nan')
    return sum((x-ma)*(y-mb) for x, y in zip(a, b))/(sa*sb)

def spearman(a, b): return pearson(ranks(a), ranks(b))

def auc(scores, labels):
    """P(score of a positive > score of a negative), ties count 1/2."""
    r = ranks(scores); npos = sum(labels); nneg = len(labels)-npos
    if npos == 0 or nneg == 0: return float('nan')
    return (sum(ri for ri, l in zip(r, labels) if l) - npos*(npos-1)/2)/(npos*nneg)

def main(path, variant=None):
    rows = defaultdict(list)
    for r in csv.reader(open(path)):
        if variant and r[3] != variant: continue
        rows[(r[1], r[3])].append(r)
    for key, rs in sorted(rows.items()):
        est, act, tot, fail, qprod, kmin, ksum = [], [], [], [], [], [], []
        informative = zero = 0
        for r in rs:
            n, cfg, a, ok = int(r[4]), int(r[5]), int(r[6]), int(r[7])
            ny = [int(x) for x in r[10:13]]; nya = [int(x) for x in r[13:16]]
            K = [[int(r[16+3*y+e]) for e in range(3)] for y in range(3)]
            k = [K[y][ELEM[a][y]] for y in range(3)]
            e_elim = sum(ny[y]-k[y] for y in range(3))
            informative += e_elim > 0
            zero += min(k) == 0
            q = 1.0
            for y in range(3): q *= k[y]/ny[y] if ny[y] else 0.0
            fail.append(0 if ok else 1); qprod.append(-q); kmin.append(-min(k)); ksum.append(e_elim)
            if ok:
                est.append(e_elim)
                act.append(sum(ny[y]-nya[y] for y in range(3)))
                tot.append(int(r[8])-int(r[9]))
        N = len(rs)
        print(f"{key[0]} [{key[1]}] decisions={N} fail_rate={sum(fail)/N:.3f} "
              f"informative={informative/N:.3f} k_y==0={zero/N:.4f}")
        if est:
            print(f"   surviving positive branches: mean est opp-elims={sum(est)/len(est):.2f} "
                  f"mean actual opp-elims={sum(act)/len(act):.2f} mean total elims={sum(tot)/len(tot):.1f}")
            print(f"   spearman(est, actual opp-elims)={spearman(est, act):.3f} "
                  f"spearman(est, total elims)={spearman(est, tot):.3f}")
        print(f"   AUC for predicting positive-branch failure: 1-prod(q)={auc(qprod, fail):.3f} "
              f"-min(k)={auc(kmin, fail):.3f} sum(n-k)={auc(ksum, fail):.3f}")

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
