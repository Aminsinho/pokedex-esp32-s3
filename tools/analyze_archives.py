#!/usr/bin/env python3
"""Measure REAL flash contribution of each IDF archive member pulled by the linker.

Approach:
  1. Parse the linker map to find all unique (archive, member) pairs pulled in.
  2. Extract each member with `ar p` and measure its ALLOCATED section sizes
     (text + rodata + data + bss) with objdump.
  3. Aggregate per archive and per member.

This avoids the BFD linker-map size reporting bug observed in the sketch
(.rodata.str1.1 'size before relaxing' inflation).
"""
import re, subprocess, os, sys, tempfile, collections

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ar_extract

MAPFILE = os.path.join(os.path.dirname(__file__), 'build_analysis', 'pokedex.ino.map')
BIN = r"C:\Users\aminj\AppData\Local\Arduino15\packages\esp32\tools\s3-gcc\2021r2-p5\bin"
OBJDUMP = os.path.join(BIN, "xtensa-esp32s3-elf-objdump.exe")

def measure_member(archive, member):
    """Return (text, rodata, data, bss) bytes for one archive member."""
    out = tempfile.NamedTemporaryFile(suffix='.o', delete=False)
    out.close()
    try:
        rc = _extract(archive, member, out.name)
        if rc != 0:
            return None
        d = subprocess.run([OBJDUMP, '-h', out.name], capture_output=True).stdout.decode()
    finally:
        os.unlink(out.name)
    text = rodata = data = bss = 0
    for line in d.splitlines():
        m = re.match(r'\s*\d+\s+(\S+)\s+([0-9a-f]+)\s', line)
        if m and m.group(1).startswith('.'):
            name, size = m.group(1), int(m.group(2), 16)
            if name.startswith(('.text', '.literal', '.flash.text', '.iram')):
                text += size
            elif name.startswith(('.rodata', '.constdata', '.flash.rodata')):
                rodata += size
            elif name.startswith(('.data', '.dram', '.rtc.data')):
                data += size
            elif name.startswith(('.bss', '.noinit', '.dram.bss')):
                bss += size
    return text, rodata, data, bss

_cache = {}

def _extract(archive, member, outpath):
    try:
        if archive in _cache:
            members, data = _cache[archive]
        else:
            members, data = ar_extract.list_members(archive)
            _cache[archive] = (members, data)
    except Exception:
        return 1
    for name, off, size in members:
        if name == member or name.endswith('/' + member) or name.endswith('\\' + member):
            with open(outpath, 'wb') as f:
                f.write(data[off:off+size])
            return 0
    return 1

def main():
    pat = re.compile(r'([A-Za-z]:[^\s()]+\.a)\(([^()]+)\)')
    pairs = set()
    with open(MAPFILE, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = pat.search(line)
            if m:
                a = m.group(1).replace('\\', '/')
                pairs.add((a, m.group(2)))
    print(f"unique (archive, member) pairs pulled by linker: {len(pairs)}\n")

    by_arch = collections.defaultdict(lambda: [0, 0, 0, 0])
    member_rows = collections.defaultdict(list)
    failed = 0
    for a, mb in sorted(pairs):
        if not os.path.exists(a):
            failed += 1
            continue
        r = measure_member(a, mb)
        if r is None:
            failed += 1
            continue
        arch = os.path.basename(a)
        for i in range(4):
            by_arch[arch][i] += r[i]
        member_rows[arch].append((sum(r[:3]), mb, r))

    print(f"{'archive':38s} {'code+rodata':>12s} {'data':>8s} {'bss':>8s} {'members':>8s}")
    tot = [0, 0, 0, 0]
    for arch in sorted(by_arch, key=lambda a: -(by_arch[a][0] + by_arch[a][1])):
        t, rd, d, b = by_arch[arch]
        for i in range(4):
            tot[i] += by_arch[arch][i]
        print(f"{arch:38s} {t+rd:12,d} {d:8,d} {b:8,d} {len(member_rows[arch]):8d}")
    print("-" * 80)
    print(f"{'TOTAL (IDF archives)':38s} {tot[0]+tot[1]:12,d} {tot[2]:8,d} {tot[3]:8,d}")
    print(f"\nTotal flash (text+rodata+data): {tot[0]+tot[1]+tot[2]:,} bytes = {(tot[0]+tot[1]+tot[2])/1024/1024:.2f} MB")
    if failed:
        print(f"WARNING: {failed} members could not be measured")

    # Top 20 members overall
    allm = []
    for arch, rows in member_rows.items():
        for s, mb, r in rows:
            allm.append((s, arch, mb))
    allm.sort(reverse=True)
    print(f"\nTOP 25 ARCHIVE MEMBERS BY SIZE:")
    for s, arch, mb in allm[:25]:
        print(f"  {s:8,d}  {arch}({mb})")

if __name__ == '__main__':
    main()
