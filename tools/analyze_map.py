#!/usr/bin/env python3
"""Analiza pokedex.ino.map y agrupa consumo de secciones flash por objeto/librería."""
import re, sys
from collections import defaultdict

MAP = r"tools\build_analysis\pokedex.ino.map"

# Secciones que cuentan como "flash" (texto + constantes)
FLASH_SECTIONS = re.compile(r'^\s+\.text|\.rodata|\.flash\.ro|\.iram0')

# Patrón de línea de asignación en el map:
# .text.foo  0x...  0xSIZE  /path/to/lib.a(obj.o)
line_re = re.compile(r'^\s+(\.\S+)\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(.+)$')

by_obj = defaultdict(int)      # archivo -> bytes
by_obj_section = defaultdict(int)  # (archivo, seccion) -> bytes

with open(MAP, encoding="utf-8", errors="replace") as f:
    for line in f:
        m = line_re.match(line)
        if not m:
            continue
        sect, size_hex, obj = m.groups()
        if not FLASH_SECTIONS.match(" " + sect):
            continue
        size = int(size_hex, 16)
        # normaliza el objeto: quita ruta Windows
        obj = obj.strip()
        # agrupa por lib.a(obj.o) o por archivo .o
        by_obj[obj] += size
        by_obj_section[(obj, sect)] += size

total = sum(by_obj.values())
print(f"TOTAL flash atribuido a objetos: {total:,} bytes ({total/1024:.1f} KB)\n")

def norm_key(obj):
    # reduce a unidad útil: nombre lib.a(obj) o archivo
    return obj

# Ordenar
items = sorted(by_obj.items(), key=lambda kv: -kv[1])
print("TOP 30 objetos por flash:")
print(f"{'BYTES':>10}  OBJETO")
for obj, size in items[:30]:
    print(f"{size:>10,}  {obj}")

print("\n\n=== AGRUPADO POR BIBLIOTECA (lib.a) ===")
by_lib = defaultdict(int)
for obj, size in by_obj.items():
    # busca libXXX.a(obj.o)
    m2 = re.search(r'([A-Za-z0-9_\-]+\.a)\(([^)]+)\)', obj)
    if m2:
        key = m2.group(1)
    else:
        key = "sketch/otro"
    by_lib[key] += size

lib_items = sorted(by_lib.items(), key=lambda kv: -kv[1])
ltot = sum(by_lib.values())
for lib, size in lib_items:
    pct = 100.0 * size / total if total else 0
    print(f"{size:>10,} ({pct:5.1f}%)  {lib}")
print(f"{'TOTAL':>10,}  {ltot:,}")
