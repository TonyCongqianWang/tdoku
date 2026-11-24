#!/usr/bin/env python3
"""Second held-out sample (test2): puzzles from the *test* split pools (same isomorphism-hash
rule as make_splits.py) that are not in research/data/test/, plus a newly generated fresh
corpus (test-split members only) and derived multi-solution sets. Used once, for revision 2.
usage: make_test2.py <repo_root> <fresh_file> [out_name] [seed_tag]
(also used for test3: every puzzle in any research/data/<split>/ directory is excluded)"""
import os, random, subprocess, sys, json, hashlib
root, fresh2 = sys.argv[1], sys.argv[2]
OUT = sys.argv[3] if len(sys.argv) > 3 else 'test2'
SEED = sys.argv[4] if len(sys.argv) > 4 else 'test2-20261124'
cats = {'kaggle': ('data/puzzles0_kaggle', 5000), 'unbiased': ('data/puzzles1_unbiased', 10000),
        '17clue': ('data/puzzles2_17_clue', 4000), 'hard': ('data/puzzles4_forum_hardest_1905', 10000),
        'hard11': ('data/puzzles5_forum_hardest_1905_11+', 3000), 'gen': ('data/puzzles8_gen_puzzles', 10000),
        'fresh': (fresh2, 20000)}
def read(p): return [l[:81] for l in open(p) if l.strip() and not l.startswith('#')]
def hashes(ps):
    out = subprocess.run([os.path.join(root, 'build/iso_hash')], input='\n'.join(ps) + '\n',
                         capture_output=True, text=True, check=True).stdout.split()
    assert len(out) == len(ps); return out
used = set()
for split in [x for x in os.listdir(os.path.join(root, 'research/data'))
              if os.path.isdir(os.path.join(root, 'research/data', x)) and x != OUT]:
    for f in [x for x in os.listdir(os.path.join(root, 'research/data', split)) if x.endswith('.txt')]:
        used |= set(hashes(read(os.path.join(root, 'research/data', split, f))))
ex_hard = set(hashes(read(os.path.join(root, 'data/puzzles5_forum_hardest_1905_11+')))) | \
          set(hashes(read(os.path.join(root, 'data/puzzles6_forum_hardest_1106'))))
out_dir = os.path.join(root, 'research/data', OUT); os.makedirs(out_dir, exist_ok=True)
man = {}
for cat, (path, n) in cats.items():
    ps = read(path if os.path.isabs(path) else os.path.join(root, path)); hs = hashes(ps)
    pool, seen = [], set()
    for p, h in zip(ps, hs):
        if int(h, 16) % 10 < 7 or h in used or h in seen: continue
        if cat == 'hard' and h in ex_hard: continue
        seen.add(h); pool.append((h, p))
    pool.sort()
    chosen = pool if len(pool) <= n else random.Random(f'{SEED}-{cat}').sample(pool, n)
    fn = os.path.join(out_dir, cat + '.txt')
    open(fn, 'w').write(''.join(p + '\n' for _, p in chosen))
    man[cat] = {'pool': len(pool), 'count': len(chosen),
                'sha256': hashlib.sha256(open(fn, 'rb').read()).hexdigest()}
    print(f'{cat:10s} pool={len(pool):7d} chosen={len(chosen)}')
json.dump(man, open(os.path.join(out_dir, 'MANIFEST.json'), 'w'), indent=1)
