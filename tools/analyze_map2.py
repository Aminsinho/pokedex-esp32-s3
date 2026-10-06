#!/usr/bin/env python3
"""Agrupar contenido flash del .map por componente (secciones de salida que van a flash)."""
import re
from collections import defaultdict

MAP = r"tools\build_analysis\pokedex.ino.map"

FLASH_SECTS = {".flash.text", ".flash.rodata", ".flash.appdesc", ".eh_frame", ".iram0.text", ".iram0.vectors"}

OUT_HDR = re.compile(r'^(\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s*$')
IN_LINE = re.compile(r'^\s+0x([0-9a-f]{8})\s+0x([0-9a-f]+)\s+(\S.+)$')

by_comp = defaultdict(int)
sect_tot = defaultdict(int)
current = None

def classify(obj):
    o = obj.replace('\\', '/')
    m = re.search(r'libraries/([^/]+)/', o)
    if m:
        return 'lib: ' + m.group(1)
    if '/core/core.a' in o:
        return 'core arduino-esp32'
    if '/sketch/' in o:
        return 'SKETCH pokedex'
    m2 = re.search(r'lib([A-Za-z0-9_\-]+)\.a\(', o)
    if m2:
        return 'idf: ' + m2.group(1)
    base = o.rsplit('/', 1)[-1]
    if 'libc.a' in o: return 'newlib: libc'
    if 'libm.a' in o: return 'newlib: libm'
    if 'libgcc.a' in o: return 'newlib: libgcc'
    if 'libnewlib.a' in o: return 'newlib: newlib'
    return 'otros: ' + base

with open(MAP, encoding="utf-8", errors="replace") as f:
    for line in f:
        mo = OUT_HDR.match(line)
        if mo:
            name = mo.group(1)
            current = name if name in FLASH_SECTS else None
            continue
        if current is None:
            continue
        mi = IN_LINE.match(line)
        if mi:
            size = int(mi.group(2), 16)
            obj = mi.group(3)
            by_comp[classify(obj)] += size
            sect_tot[current] += size

print("=== TOTALES POR SECCIÓN DE SALIDA (flash) ===")
for s in [".flash.text", ".flash.rodata", ".eh_frame", ".flash.appdesc", ".iram0.text", ".iram0.vectors"]:
    print(f"  {s:<16} {sect_tot.get(s,0):>12,} bytes")
tot = sum(sect_tot.values())
print(f"  {'TOTAL':<16} {tot:>12,} bytes\n")

print("=== CONSUMO POR COMPONENTE (flash) ===")
for name, size in sorted(by_comp.items(), key=lambda kv: -kv[1]):
    print(f"  {size:>10,} ({100.0*size/tot:5.1f}%)  {name}")
