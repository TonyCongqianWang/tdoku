#!/usr/bin/env python3
"""Build isomorphism-disjoint dev / validation / test splits from tdoku's benchmark corpora.

Every puzzle is mapped to a symmetry-invariant hash (build/iso_hash, see research/tools).
The split of a puzzle is a fixed function of its hash, so isomorphic copies of a puzzle -- in
the same file or across files (e.g. forum_hardest_1905 vs its 11+ subset vs the 1106 list) --
always land in the same split. Each split/category file is then a seeded sample from that
split's pool. Puzzles are written exactly as in the source (no transformation); the benchmark
driver applies seeded random transformations at load time.

usage: make_splits.py <repo_root> <out_dir> [fresh_generated_file]
"""
import hashlib, os, random, subprocess, sys, json

SPLIT_OF = {0: 'dev', 1: 'dev', 2: 'dev', 3: 'dev', 4: 'val', 5: 'val', 6: 'val',
            7: 'test', 8: 'test', 9: 'test'}

# category -> (source file, {split: sample size}); None = take everything in the pool
CATEGORIES = {
    'kaggle':    ('data/puzzles0_kaggle',               {'dev': 2000, 'val': 2000, 'test': 10000}),
    'unbiased':  ('data/puzzles1_unbiased',             {'dev': 10000, 'val': 10000, 'test': 20000}),
    '17clue':    ('data/puzzles2_17_clue',              {'dev': 5000, 'val': 5000, 'test': 10000}),
    'magictour': ('data/puzzles3_magictour_top1465',    {'dev': None, 'val': None, 'test': None}),
    'hard':      ('data/puzzles4_forum_hardest_1905',   {'dev': 10000, 'val': 10000, 'test': 20000}),
    'hard11':    ('data/puzzles5_forum_hardest_1905_11+', {'dev': 3000, 'val': 3000, 'test': 5000}),
    'hard1106':  ('data/puzzles6_forum_hardest_1106',   {'dev': None, 'val': None, 'test': None}),
    'serg':      ('data/puzzles7_serg_benchmark',       {'dev': 2000, 'val': 2000, 'test': 3000}),
    'gen':       ('data/puzzles8_gen_puzzles',          {'dev': 10000, 'val': 10000, 'test': 20000}),
}
SEED = 20261123


def read_puzzles(path):
    out = []
    for line in open(path):
        if not line.strip() or line.startswith('#'):
            continue
        out.append(line[:81])
    return out


def iso_hashes(root, puzzles):
    res = subprocess.run([os.path.join(root, 'build/iso_hash')], input='\n'.join(puzzles) + '\n',
                         capture_output=True, text=True, check=True)
    hs = res.stdout.split()
    assert len(hs) == len(puzzles)
    return hs


def main():
    root, out_dir = sys.argv[1], sys.argv[2]
    fresh = sys.argv[3] if len(sys.argv) > 3 else None
    cats = dict(CATEGORIES)
    if fresh:
        cats['fresh'] = (fresh, {'dev': 0, 'val': 0, 'test': None})
    # hashes of the hard11 / hard1106 lists: excluded from 'hard' so categories are disjoint
    loaded = {}
    for cat, (path, _) in cats.items():
        p = read_puzzles(os.path.join(root, path) if not os.path.isabs(path) else path)
        loaded[cat] = (p, iso_hashes(root, p))
    exclude_from_hard = set(loaded['hard11'][1]) | set(loaded['hard1106'][1])
    manifest = {'seed': SEED, 'split_rule': 'int(iso_hash,16) % 10 -> 0-3 dev, 4-6 val, 7-9 test',
                'files': {}}
    seen_global = {}
    for cat, (path, sizes) in cats.items():
        puzzles, hashes = loaded[cat]
        pools = {'dev': [], 'val': [], 'test': []}
        seen = set()
        for p, h in zip(puzzles, hashes):
            if h in seen:           # drop isomorphic duplicates within a category
                continue
            if cat == 'hard' and h in exclude_from_hard:
                continue
            seen.add(h)
            pools[SPLIT_OF[int(h, 16) % 10]].append((h, p))
        for split, pool in pools.items():
            n = sizes[split]
            if n == 0:
                continue
            pool.sort()
            rng = random.Random(f'{SEED}-{cat}-{split}')
            chosen = pool if n is None or n >= len(pool) else rng.sample(pool, n)
            for h, _ in chosen:
                prev = seen_global.get(h)
                assert prev is None or prev == split, f'leak {cat} {h} {prev} {split}'
                seen_global[h] = split
            os.makedirs(os.path.join(out_dir, split), exist_ok=True)
            fn = os.path.join(out_dir, split, cat + '.txt')
            with open(fn, 'w') as f:
                for h, p in chosen:
                    f.write(p + '\n')
            data = open(fn, 'rb').read()
            manifest['files'][f'{split}/{cat}.txt'] = {
                'source': path, 'source_sha256': hashlib.sha256(
                    open(os.path.join(root, path) if not os.path.isabs(path) else path, 'rb').read()
                ).hexdigest(),
                'pool_size': len(pool), 'count': len(chosen),
                'sha256': hashlib.sha256(data).hexdigest()}
            print(f'{split:5s} {cat:10s} pool={len(pool):8d} chosen={len(chosen):6d}')
    json.dump(manifest, open(os.path.join(out_dir, 'MANIFEST.json'), 'w'), indent=1, sort_keys=True)


if __name__ == '__main__':
    main()
