#!/usr/bin/env python3
"""
Pruebas de aceptación de los datos PKME (mecánicas por generación).

Valida los binarios de muestra generados por tools/generate_mechanics.py:
  - formato + cabecera + CRC + payload_size
  - cadena evolutiva lineal   (Bulbasaur -> Ivysaur -> Venusaur)
  - cadena evolutiva bifurcada (Eevee -> 8 evoluciones)
  - preevolución              (Raichu muestra Pichu -> Pikachu -> Raichu)
  - generación no disponible  (Pichu: sin Gen I)
  - nivel 0 = INICIO
  - dedupe + orden por nivel
  - nombres UTF-8 no vacíos
  - rechazo: truncado, CRC erróneo, versión desconocida

Ejecuta:  python tests/test_mechanics.py
Salida 0 = todo PASS; 1 = algún fallo.
"""

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import tools.generate_mechanics as g  # noqa: E402

DATA_DIR = ROOT / "sd_dataset" / "pokemon" / "mechanics" / "data"

PASS = 0
FAIL = 0


def check(name: str, cond: bool, detail: str = ""):
    global PASS, FAIL
    if cond:
        PASS += 1
        print(f"  PASS  {name}")
    else:
        FAIL += 1
        print(f"  FAIL  {name}   {detail}")


def load(pokemon_id: int) -> bytes:
    p = DATA_DIR / f"{pokemon_id:04d}.bin"
    if not p.exists():
        raise FileNotFoundError(f"falta el binario de muestra {p} "
                               f"(ejecuta: python tools/generate_mechanics.py --ids 1,25,26,133,172)")
    return p.read_bytes()


def root_and_children(parsed: dict):
    nodes = parsed["nodes"]
    by_idx = {i: n for i, n in enumerate(nodes)}
    roots = [i for i, n in enumerate(nodes) if n[2] == -1]
    return nodes, by_idx, roots


def main():
    print("=== Pruebas PKME (mecánicas por generación) ===")

    # ---- Carga de las especies de muestra ---------------------------------
    try:
        blobs = {pid: load(pid) for pid in (1, 25, 26, 133, 172, 151)}
    except FileNotFoundError as e:
        print(f"  ERROR {e}")
        return 1

    parsed = {}
    for pid, blob in blobs.items():
        parsed[pid] = g.parse_pkme(blob)

    # ---- 1. Formato + cabecera + CRC + payload_size -----------------------
    print("\n[1] Formato / cabecera / CRC / payload_size")
    for pid, pr in parsed.items():
        check(f"{pid:04d} valid+magic+version+CRC", pr["valid"] and pr["id"] == pid,
              f"valid={pr['valid']}")
    # payload_size coherente con el tamaño del archivo
    for pid, blob in blobs.items():
        (_, _, payload_size, _, _, _, _) = struct.unpack_from("<4sHHIIHBB", blob, 0)[1:]
        check(f"{pid:04d} payload_size == len-20", payload_size == len(blob) - 20,
              f"{payload_size} vs {len(blob)-20}")

    # ---- 2. Cadena lineal (Bulbasaur) -------------------------------------
    print("\n[2] Cadena evolutiva lineal: Bulbasaur -> Ivysaur -> Venusaur")
    p = parsed[1]
    nodes, by_idx, roots = root_and_children(p)
    check("3 nodos", len(nodes) == 3, f"n={len(nodes)}")
    check("una sola raíz", len(roots) == 1, f"roots={len(roots)}")
    if len(roots) == 1:
        ridx = roots[0]
        root = nodes[ridx]
        check("raíz = Bulbasaur (id=1)", root[0] == 1, f"root_id={root[0]}")
        # el hijo de la raíz
        kids = [i for i, n in enumerate(nodes) if n[2] == ridx]
        check("raíz tiene 1 hijo", len(kids) == 1, f"kids={len(kids)}")
        if kids:
            kid = nodes[kids[0]]
            check("hijo = Ivysaur (id=2)", kid[0] == 2, f"id={kid[0]}")
            # el nieto
            gkids = [i for i, n in enumerate(nodes) if n[2] == kids[0]]
            check("Ivysaur tiene 1 hijo", len(gkids) == 1, f"{len(gkids)}")
            if gkids:
                check("nieto = Venusaur (id=3)", nodes[gkids[0]][0] == 3,
                      f"id={nodes[gkids[0]][0]}")

    # ---- 2b. Sin evolución (Mew) ------------------------------------------
    print("\n[2b] Sin evolución: Mew es un único nodo raíz")
    p = parsed[151]
    nodes, by_idx, roots = root_and_children(p)
    check("1 nodo", len(nodes) == 1, f"n={len(nodes)}")
    check("raíz = Mew (id=151) sin padre", len(roots) == 1 and nodes[0][0] == 151
          and nodes[0][2] == -1, f"{nodes}")

    # ---- 3. Cadena bifurcada (Eevee) --------------------------------------
    print("\n[3] Cadena evolutiva bifurcada: Eevee -> 8 evoluciones")
    p = parsed[133]
    nodes, by_idx, roots = root_and_children(p)
    check("9 nodos (Eevee + 8)", len(nodes) == 9, f"n={len(nodes)}")
    check("una sola raíz", len(roots) == 1, f"roots={len(roots)}")
    if len(roots) == 1:
        ridx = roots[0]
        check("raíz = Eevee (id=133)", nodes[ridx][0] == 133, f"id={nodes[ridx][0]}")
        kids = [n[0] for n in nodes if n[2] == ridx]
        check("Eevee tiene 8 hijos", len(kids) == 8, f"kids={sorted(kids)}")
        # los 8 no deben evolucionar entre sí (todos hijos directos de Eevee)
        check("8 hijos distintos", len(set(kids)) == 8, f"{sorted(kids)}")

    # ---- 4. Preevolución (Raichu) -----------------------------------------
    print("\n[4] Preevolución: la ficha de Raichu muestra Pichu -> Pikachu -> Raichu")
    p = parsed[26]
    nodes, by_idx, roots = root_and_children(p)
    ids = {n[0] for n in nodes}
    check("incluye Raichu (26)", 26 in ids, f"{ids}")
    check("incluye Pikachu (25) como ancestro", 25 in ids, f"{ids}")
    check("incluye Pichu (172) como ancestro", 172 in ids, f"{ids}")
    check("una sola raíz", len(roots) == 1, f"roots={len(roots)}")
    if len(roots) == 1:
        check("raíz = Pichu (172)", nodes[roots[0]][0] == 172, f"id={nodes[roots[0]][0]}")

    # ---- 5. Generación no disponible (Pichu: sin Gen I) --------------------
    print("\n[5] Disponibilidad por generación: Pichu (Gen II) no tiene Gen I")
    p = parsed[172]
    check("máscara NO incluye Gen I", not (p["mask"] & (1 << 0)), f"mask={p['mask']:016b}")
    check("máscara SÍ incluye Gen II", (p["mask"] & (1 << 1)) != 0, f"mask={p['mask']:016b}")
    check("generation_count == bits activos",
          p["generation_count"] == bin(p["mask"]).count("1"),
          f"count={p['generation_count']} bits={bin(p['mask']).count('1')}")
    # Bulbasaur (Gen I) SÍ tiene Gen I
    check("Bulbasaur SÍ tiene Gen I", (parsed[1]["mask"] & (1 << 0)) != 0)

    # ---- 6. Nivel 0 = INICIO ----------------------------------------------
    print("\n[6] Nivel 0 = INICIO (al menos un movimiento con level==0)")
    found_level0 = False
    for pid, pr in parsed.items():
        for (gen, vcode, moves) in pr.get("sections", []):
            for (level, order, mid, tcode, flags, name, power, accuracy, pp, category, description) in moves:
                if level == 0:
                    found_level0 = True
                    break
    check("existe algún movimiento de nivel 0", found_level0)

    # ---- 7. Dedupe + orden por nivel --------------------------------------
    print("\n[7] Dedupe y orden por nivel")
    ok_dedupe = True
    ok_order = True
    for pid, pr in parsed.items():
        for (gen, vcode, moves) in pr.get("sections", []):
            seen = set()
            levels = [m[0] for m in moves]
            if levels != sorted(levels):
                ok_order = False
            for (level, order, mid, tcode, flags, name, power, accuracy, pp, category, description) in moves:
                key = (level, mid)
                if key in seen:
                    ok_dedupe = False
                seen.add(key)
    check("niveles no decrecientes en cada generación", ok_order)
    check("sin duplicados (nivel, movimiento)", ok_dedupe)

    # ---- 8. Nombres UTF-8 no vacíos ---------------------------------------
    print("\n[8] Nombres de movimiento UTF-8 no vacíos")
    all_named = True
    has_accent = False
    for pid, pr in parsed.items():
        for (gen, vcode, moves) in pr.get("sections", []):
            for (level, order, mid, tcode, flags, name, power, accuracy, pp, category, description) in moves:
                if not name:
                    all_named = False
                if any(ord(c) > 0x7F for c in name):
                    has_accent = True
    check("ningún nombre vacío", all_named)
    check("hay nombres en español (con acento/UTF-8)", has_accent)

    print("\n[8b] Intercambio detectado desde evolution trigger")
    machoke = g.parse_pkme((DATA_DIR / "0067.bin").read_bytes())
    machamp = next(node for node in machoke["nodes"] if node[0] == 68)
    check("Machoke -> Machamp usa condición intercambio", any(c[0] == 3 for c in machamp[3]))

    print("\n[8c] Amistad distingue día y noche")
    eevee = g.parse_pkme((DATA_DIR / "0133.bin").read_bytes())
    espeon = next(node for node in eevee["nodes"] if node[0] == 196)
    umbreon = next(node for node in eevee["nodes"] if node[0] == 197)
    check("Espeon = amistad durante el día", any(c[0] == 15 for c in espeon[3]))
    check("Umbreon = amistad durante la noche", any(c[0] == 16 for c in umbreon[3]))

    # ---- 9. Rechazos: truncado / CRC / versión ----------------------------
    print("\n[9] Casos de rechazo (truncado, CRC erróneo, versión errónea)")
    base = blobs[1]
    # truncado
    check("rechaza truncado", not g.parse_pkme(base[:len(base) - 5])["valid"])
    # CRC erróneo: corrompe un byte del payload
    bad_crc = bytearray(base)
    bad_crc[-1] ^= 0xFF  # último byte del payload
    check("rechaza CRC erróneo", not g.parse_pkme(bytes(bad_crc))["valid"])
    # versión errónea
    bad_ver = bytearray(base)
    bad_ver[5] ^= 0x01  # version u16 (offset 4..5)
    check("rechaza versión desconocida", not g.parse_pkme(bytes(bad_ver))["valid"])
    # magic erróneo
    bad_magic = bytearray(base)
    bad_magic[0] = 0x00
    check("rechaza magic inválido", not g.parse_pkme(bytes(bad_magic))["valid"])

    # ---- Resumen -----------------------------------------------------------
    print(f"\n=== {PASS} PASS, {FAIL} FAIL ===")
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
