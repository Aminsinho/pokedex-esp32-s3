import re, os, sys, collections
sys.path.insert(0, r'tools')
import ar_extract
pat = re.compile(r'([A-Za-z]:[^\s()]+\.a)\(([^()]+)\)')
pairs = set()
with open(r'tools\build_analysis\pokedex.ino.map', encoding='utf-8', errors='replace') as f:
    for line in f:
        m = pat.search(line)
        if m:
            a = m.group(1).replace('\\', '/')
            pairs.add((a, m.group(2)))
print('unique pairs:', len(pairs))
cache = {}
ok = collections.Counter()
fail = []
for a, mb in sorted(pairs):
    if not os.path.exists(a):
        fail.append((a, mb, 'no archive'))
        continue
    try:
        if a not in cache:
            cache[a] = ar_extract.list_members(a)
        members, data = cache[a]
    except Exception as e:
        fail.append((a, mb, 'parse error: %s' % e))
        continue
    found = any(n == mb or n.endswith('/' + mb) or n.endswith('\\' + mb) for n, _, _ in members)
    if found:
        ok[os.path.basename(a)] += 1
    else:
        fail.append((a, mb, 'member not in archive'))
print('OK pairs:', sum(ok.values()))
print('FAIL pairs:', len(fail))
print('fails by archive:')
for a, c in collections.Counter(os.path.basename(x[0]) for x in fail).most_common(15):
    print('  %4d  %s' % (c, a))
print('sample fails:')
for x in fail[:12]:
    print('  ', os.path.basename(x[0]), x[1], x[2])
