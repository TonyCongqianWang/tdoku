# extract each k_* function's fast path (entry to first ret) from a gcc -S file
import sys, re
lines = open(sys.argv[1]).read().split('\n')
out = {}
cur = None
for l in lines:
    m = re.match(r'^(k_\w+):', l)
    if m: cur = m.group(1); out[cur] = []; continue
    if cur is None: continue
    s = l.strip()
    if not s or s.startswith('.') and not s.startswith('.L'): continue
    if s.startswith('.L'): continue  # labels
    if s.startswith('ret'):
        open(f'{sys.argv[2]}/{cur}.s', 'w').write('\n'.join(out[cur]) + '\n'); cur = None; continue
    out[cur].append(s)
