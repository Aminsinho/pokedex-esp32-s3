"""
Final flash/RAM attribution from the linker map (analyze_map3).

Key discovery: the map's input-section size field is unreliable for some
entries (BFD relaxation reporting bug). When the linker emits the annotation:
    <real size> (size before relaxing)
that annotation is the TRUE size. We use it when present, else the size field.
"""
import re, collections

MAP = r'tools\build_analysis\pokedex.ino.map'

IN_SIZE = re.compile(r'^\s*(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+([^\s]+)\s*$')
IN_RELY = re.compile(r'^\s*(0x[0-9a-f]+)\s+\(size before relaxing\)\s*$')
IN_HDR = re.compile(r'^\.(\S+)\s*$')

def classify(sec):
    if sec.startswith('.bss'):
        return 'bss'
    if sec.startswith(('.data', '.sdata', '.data.rel')):
        return 'dram'
    if sec.startswith(('.text', '.literal', '.rodata', '.srodata', '.xt.prop', '.xt.lit')):
        return 'text'
    if sec.startswith('.eh_frame'):
        return 'ehframe'
    return 'other'

per_obj = collections.defaultdict(lambda: collections.Counter())
per_archive = collections.defaultdict(lambda: collections.Counter())
per_member = collections.defaultdict(lambda: collections.Counter())
total = collections.Counter()
n_entries = 0
n_rely = 0

with open(MAP, encoding='utf-8', errors='replace') as f:
    lines = [l.rstrip('\n') for l in f]

start = 0
for i, l in enumerate(lines):
    if l.strip() == 'Memory Map':
        start = i
        break
end = len(lines)
for i, l in enumerate(lines[start:], start):
    if l.strip() == 'Discarded input sections':
        end = i
        break

for i in range(start, end):
    m = IN_SIZE.match(lines[i])
    if not m:
        continue
    # origin must end with .o or .obj (not an output section header line)
    origin = m.group(3)
    if not (origin.endswith('.o') or origin.endswith('.obj')):
        continue
    size = int(m.group(2), 16)
    # find input section name in previous lines
    j = i - 1
    secname = None
    while j >= start:
        lj = lines[j].strip()
        mh = IN_HDR.match(lj)
        if mh:
            secname = mh.group(1)
            break
        if IN_SIZE.match(lines[j]) and (lines[j].strip().endswith('.o') or lines[j].strip().endswith('.obj')):
            break
        j -= 1
    if secname is None:
        continue
    # peek next line for "size before relaxing"
    if i + 1 < end:
        mm = IN_RELY.match(lines[i + 1])
        if mm:
            size = int(mm.group(1), 16)
            n_rely += 1
    cls = classify(secname)
    # extract member (archive(name)) if present — origin may be "archive.a(member)"
    member = None
    pm = re.match(r'^(.*)\(([^()]*)\)$', origin)
    aname = origin
    if pm:
        aname = pm.group(1)
        member = pm.group(2)
    aname = aname.replace('\\', '/').split('/')[-1]
    per_obj[origin][cls] += size
    per_obj[origin]['total'] += size
    per_archive[aname][cls] += size
    per_archive[aname]['total'] += size
    if member:
        per_member[(aname, member)][cls] += size
        per_member[(aname, member)]['total'] += size
    total[cls] += size
    total['total'] += size
    n_entries += 1

print('input-section entries attributed: %d  (of which %d used "size before relaxing")' % (n_entries, n_rely))
print()
print('TOTAL by class:')
for k in ('text', 'dram', 'bss', 'ehframe', 'other', 'total'):
    print('  %-8s %12d' % (k, total.get(k, 0)))
print()
print('PER OBJECT (top 35 by total):')
for o, c in sorted(per_obj.items(), key=lambda kv: -kv[1]['total'])[:35]:
    print('  %-80s tot=%8d text=%8d dram=%6d bss=%6d' % (o.replace('\\', '/')[-80:], c['total'], c.get('text', 0), c.get('dram', 0), c.get('bss', 0)))
print()
print('PER ARCHIVE (sorted by total):')
for a, c in sorted(per_archive.items(), key=lambda kv: -kv[1]['total']):
    print('  %-38s tot=%10d  text=%10d  dram=%8d  bss=%8d' % (a, c['total'], c.get('text', 0), c.get('dram', 0), c.get('bss', 0)))
print()
print('TOP 30 ARCHIVE MEMBERS:')
for (a, mbr), c in sorted(per_member.items(), key=lambda kv: -kv[1]['total'])[:30]:
    print('  %-35s %-35s tot=%8d' % (a, mbr, c['total']))
