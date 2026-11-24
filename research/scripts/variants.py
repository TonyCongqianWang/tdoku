#!/usr/bin/env python3
"""Named variant specs for research_bench (-v).  `python3 variants.py <group>` prints a -v string.

Research-selector parameters (see TdokuResearchParams in src/branch_features.h):
  pair: 0 none, 1 original (total, band idx, {2},{3},{4+}), 2 exact (total, n), 3 flat n,
        4 flat (n, total), 5 soft M_p^w(n, total/9)
  ix:   0 first config, 1 sum k, 2 genmean q (ixp=p; <=-100 = min), 3 G=(1-prod q)/n,
        4 two-sided product, 5 two-sided sum, 6 lookahead product (actual propagation)
  scope: 0 score tuples of every pair in the best class, 1 only configs of the first pair
"""
import sys

IX = {'sum': 'ix=1', 'am': 'ix=2,ixp=1', 'gm': 'ix=2,ixp=0', 'hm': 'ix=2,ixp=-1',
      'p-4': 'ix=2,ixp=-4', 'min': 'ix=2,ixp=-1000', 'G': 'ix=3'}


def groups():
    g = {}
    g['A'] = {'A_orig': 'h:0', 'A_exact': 'r:pair=2'}
    g['B'] = {'B_flat': 'r:pair=3', 'B_flatlex': 'r:pair=4'}
    g['C'] = {f'C_w{w}_p{p}': f'r:pair=5,w={w},p={p}'
              for w in (0.1, 0.25, 0.5, 0.75, 0.9) for p in (-1, 0, 1)}
    g['Dcfg'] = {f'Dcfg_{k}': f'r:pair=1,scope=1,{v}' for k, v in IX.items()}
    g['Dtie_orig'] = {f'Dtie_orig_{k}': f'r:pair=1,scope=0,{v}' for k, v in IX.items()}
    g['Dtie_flat'] = {f'Dtie_flat_{k}': f'r:pair=3,scope=0,{v}' for k, v in IX.items()}
    g['Djoint'] = {'Djoint_G': 'r:pair=0,ix=3'}
    g['E'] = {'E_orig_prod': 'r:pair=1,ix=4', 'E_orig_sum': 'r:pair=1,ix=5',
              'E_flat_prod': 'r:pair=3,ix=4', 'E_flat_sum': 'r:pair=3,ix=5',
              'E_joint_prod': 'r:pair=0,ix=4', 'E_joint_sum': 'r:pair=0,ix=5'}
    g['E2'] = {'E_orig_am2': 'r:pair=1,ix=7', 'E_flat_am2': 'r:pair=3,ix=7',
               'E_flat_ammax': 'r:pair=3,ix=8', 'E_orig_ammax': 'r:pair=1,ix=8'}
    g['key'] = {'A_orig': 'h:0', 'B_flat': 'r:pair=3', 'Dcfg_am': 'r:pair=1,scope=1,ix=2,ixp=1',
                'Dtie_orig_am': 'r:pair=1,ix=2,ixp=1', 'Dtie_flat_am': 'r:pair=3,ix=2,ixp=1',
                'E_orig_am2': 'r:pair=1,ix=7', 'E_flat_am2': 'r:pair=3,ix=7',
                'E_flat_prod': 'r:pair=3,ix=4'}
    g['opt'] = {'IX3': 'h:3', 'G_f1o1': 'g:1:0:1', 'G_f1o2': 'g:1:0:2', 'G_f1o4': 'g:1:0:4',
                'FINAL': 'h:5'}
    g['final'] = {'A_orig': 'h:0', 'IX3': 'h:3', 'FINAL': 'h:5'}
    g['score2'] = {'AM2': 'r:pair=3,ix=7', 'GM2': 'r:pair=3,ix=9',
                   'SQ2': 'r:pair=3,ix=10,ixp=2', 'SQRT2': 'r:pair=3,ix=10,ixp=0.5',
                   'PROD2': 'r:pair=3,ix=11', 'INF2': 'r:pair=3,ix=12',
                   'AM2_NB.05': 'r:pair=3,ix=13,ixp=0.05', 'AM2_NB.5': 'r:pair=3,ix=13,ixp=0.5',
                   'CELL2': 'r:pair=3,ix=14', 'AM2_CELL.5': 'r:pair=3,ix=15,ixp=0.5',
                   'AM2_CELL1': 'r:pair=3,ix=15,ixp=1', 'AM2_CELL2': 'r:pair=3,ix=15,ixp=2',
                   'AM2_LOGW': 'r:pair=3,ix=16'}
    g['Look'] = {'Look_orig': 'r:pair=1,ix=6', 'Look_flat': 'r:pair=3,ix=6',
                 'Look_joint': 'r:pair=0,ix=6'}
    return g


def spec(names):
    g = groups()
    out = {}
    for name in names:
        out.update(g[name])
    return ';'.join(f'{k}={v}' for k, v in out.items())


if __name__ == '__main__':
    print(spec(sys.argv[1:]))
