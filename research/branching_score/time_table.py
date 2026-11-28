"""Tabulate run_time.sh output: median ns per puzzle and ratio to the original solver.
usage: python3 time_table.py <time_*.txt>"""
import math
import sys

rows, cur = {}, None
for line in open(sys.argv[1]):
    t = line.split()
    if line.startswith("##"):
        cur = (t[2], t[4])
        continue
    if not t:
        continue
    d = dict(zip(t[1::2], t[2::2]))
    rows.setdefault(cur, {})[t[0]] = (float(d["median_ns"]), float(d["guesses"]), float(d["min_ns"]))
pols = ["orig", "ixg", "ds", "dv2", "lv2"]
for limit in ("2", "1"):
    keys = [k for k in rows if k[1] == limit]
    if not keys:
        continue
    print("limit %s: median ns/puzzle (time ratio vs orig) [guesses/puzzle]" % limit)
    print("%-10s" % "set" + "".join("%26s" % p for p in pols))
    logs = {p: [] for p in pols}
    for k in keys:
        r = rows[k]
        line = "%-10s" % k[0]
        for p in pols:
            ns, g, _ = r[p]
            line += "%11.0f (%5.3f) [%7.1f]" % (ns, ns / r["orig"][0], g)
            logs[p].append(math.log(ns / r["orig"][0]))
        print(line)
    print("%-10s" % "geomean" + "".join("%26s" % ("(%5.3f)" % math.exp(sum(v) / len(v))) for v in logs.values()))
    print()
