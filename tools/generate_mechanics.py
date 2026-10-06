#!/usr/bin/env python3
"""
Generador de datos PKME (mecánicas por generación) para el Pokédex ESP32-S3.

Output (dentro de sd_dataset/pokemon/mechanics/):
  manifest.json
  data/NNNN.bin   - una especie por archivo, legible bajo demanda

Fuente: PokéAPI v2 (descarga con caché local, reintentos acotados).
PokéAPI NUNCA se consulta desde el ESP32: todo se resuelve aquí, en el PC.

Formato PKME versión 2  (little-endian, cadenas UTF-8 sin terminador):

  Cabecera fija (20 bytes):
    magic            char[4]   = "PKME"
    version          uint16    = 2
    pokemon_id       uint16    = ID nacional (1..1025)
    payload_size     uint32    = bytes después de la cabecera
    crc32            uint32    = CRC-32 (IEEE 802.3) del payload
    generation_mask  uint16    = bits 0..8 = generaciones I..IX
    generation_count uint8     = nº de secciones incluidas
    reserved         uint8     = 0

  Cadena evolutiva común:
    node_count       uint8
    Por nodo (5 + condiciones):
      species_id             uint16
      introduced_generation  uint8   (1..9)
      parent_index           int8    (-1 = raíz)
      condition_count        uint8
      Por condición (5 + texto):
        kind                  uint8
        introduced_generation uint8
        value_u16             uint16 (nivel, item, movimiento, ...)
        text_len              uint8
        text_utf8[text_len]

  Sección por generación (por cada bit activo, ascendente):
    generation        uint8
    version_group_code uint8
    move_count        uint16
    Por movimiento (8 + nombre):
      level     uint8   (0 = INICIO)
      order     uint16  (orden de la fuente dentro del nivel)
      move_id   uint16
      type_code uint8   (1..18, 0 = desconocido)
      flags     uint8   (bit0: nombre en inglés como respaldo)
      name_len  uint8
      name_utf8[name_len]
      power, accuracy, pp, category uint8 cada uno (255 = no aplica)
      description_len uint8
      description_utf8[description_len]

Límites de validación (firmware los rechaza): >255 nodos, >512 movimientos
por generación, IDs fuera de rango, longitudes que excedan el archivo,
máscara incoherente, versión desconocida o CRC erróneo.

Uso:
  python tools/generate_mechanics.py --first 1 --last 10
  python tools/generate_mechanics.py --ids 1,25,26,133,172
  python tools/generate_mechanics.py --offline --first 1 --last 10   # solo caché
"""

import argparse
import datetime
import hashlib
import json
import re
import struct
import sys
import time
import urllib.request
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CACHE_DIR = ROOT / "tools" / "gen_cache"
OUTPUT_DIR = ROOT / "sd_dataset" / "pokemon" / "mechanics"
SPECIES_JSON = ROOT / "backend" / "app" / "data" / "pokemon.json"
MOVE_TRANSLATIONS_JSON = ROOT / "tools" / "move_descriptions_es.json"
MOVE_TRANSLATIONS = (json.loads(MOVE_TRANSLATIONS_JSON.read_text(encoding="utf-8"))
                     if MOVE_TRANSLATIONS_JSON.exists() else {})

API_BASE = "https://pokeapi.co/api/v2"
USER_AGENT = "Mozilla/5.0 (PokedexTool; pokedex-firmware)"
MAX_RETRIES = 3
RETRY_DELAY = 1.5

# ----------------------------------------------------------------------------
# Constantes de formato
# ----------------------------------------------------------------------------
PKME_MAGIC = b"PKME"
PKME_VERSION = 2

# Tabla canónica generación -> version group (versionada en el generador).
GEN_VGROUP = {
    1: "red-blue",
    2: "crystal",
    3: "emerald",
    4: "platinum",
    5: "black-2-white-2",
    6: "omega-ruby-alpha-sapphire",
    7: "ultra-sun-ultra-moon",
    8: "sword-shield",
    9: "scarlet-violet",
}
VGROUP_CODE = {name: gen for gen, name in GEN_VGROUP.items()}  # code == generación

# Tipos de Pokémon -> uint8 code (igual mapeo que el generador PKDP).
TYPE_MAP = {
    "normal": 1, "fire": 2, "water": 3, "electric": 4,
    "grass": 5, "ice": 6, "fighting": 7, "poison": 8,
    "ground": 9, "flying": 10, "psychic": 11, "bug": 12,
    "rock": 13, "ghost": 14, "dragon": 15, "dark": 16,
    "steel": 17, "fairy": 18,
}

# Kind de condición de evolución (campo uint8 del binario).
COND_SPECIAL = 0       # "Condición especial" (fuente sin datos reconocibles)
COND_LEVEL = 1
COND_ITEM = 2
COND_TRADE = 3
COND_FRIENDSHIP = 4
COND_TIME = 5
COND_GENDER = 6
COND_MOVE_KNOWN = 7
COND_TYPE_KNOWN = 8
COND_LOCATION = 9
COND_WEATHER = 10
COND_PARTY = 11
COND_EQUIP = 12
COND_BEAUTY = 13
COND_AFFECTION = 14
COND_FRIENDSHIP_DAY = 15
COND_FRIENDSHIP_NIGHT = 16

# Mapeo de items de evolución (slug PokeAPI → nombre en español).
ITEM_ES = {
    "fire-stone":        "Piedra Fuego",
    "water-stone":       "Piedra Agua",
    "thunder-stone":     "Piedra Trueno",
    "leaf-stone":        "Piedra Planta",
    "moon-stone":        "Piedra Luna",
    "sun-stone":         "Piedra Sol",
    "dawn-stone":        "Piedra Alba",
    "dusk-stone":        "Piedra Crepúsculo",
    "ice-stone":         "Piedra Hielo",
    "oval-stone":        "Piedra Oval",
    "dragon-scale":      "Escama Dragón",
    "prism-scale":       "Escama Prisma",
    "deep-sea-scale":    "Escama Abisal",
    "deep-sea-tooth":    "Diente Abisal",
    "metal-coat":        "Revest. Metálico",
    "metal-alloy":       "Aleación Metálica",
    "up-grade":          "Ascensor",
    "razor-fang":        "Colmillo Afilado",
    "razor-claw":        "Garra Afilada",
    "kings-rock":        "Roca del Rey",
    "electirizer":       "Electrificador",
    "magmarizer":        "Magmificador",
    "dubious-disc":      "Disco Dúbio",
    "protector":         "Protector",
    "reaper-cloth":      "Paño Siniestro",
    "sachet":            "Saché",
    "scroll-of-darkness": "Pergamino Oscuridad",
    "scroll-of-waters":  "Pergamino Aguas",
    "whipped-dream":     "Sueño Batido",
    "sweet-apple":       "Manzana Dulce",
    "tart-apple":        "Manzana Ácida",
    "syrupy-apple":      "Manzana Siruposa",
    "shiny-stone":       "Piedra Brillante",
    "black-augurite":    "Augurita Negra",
    "galarica-cuff":     "Galarica",
    "galarica-wreath":   "Corona Galarica",
    "chipped-pot":       "Maceta Grieta",
    "cracked-pot":       "Maceta Agrietada",
    "unremarkable-teacup": "Taza Sencilla",
    "masterpiece-teacup":  "Taza Maestra",
    "peat-block":        "Bloque de Turba",
    "malicious-armor":   "Armadura Maliciosa",
    "auspicious-armor":  "Armadura Propicia",
}

MOVE_FLAG_EN_NAME = 0x01  # nombre guardado en inglés (no había oficial ES)
MOVE_FLAG_EN_DESCRIPTION = 0x02
MAX_MOVE_DESCRIPTION_BYTES = 180

# Límites (el firmware los aplica como validación defensiva).
MAX_NODES = 255
MAX_MOVES_PER_GEN = 512
MAX_NAME_BYTES = 255
MIN_SPECIES_ID = 1
MAX_SPECIES_ID = 1025


# ----------------------------------------------------------------------------
# Capa de red + caché
# ----------------------------------------------------------------------------
def _cache_path(url: str) -> Path:
    digest = hashlib.sha1(url.encode("utf-8")).hexdigest()
    return CACHE_DIR / f"{digest}.json"


def fetch_json(url: str, offline: bool = False):
    """Descarga JSON de PokéAPI con caché en disco y reintentos acotados."""
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    cp = _cache_path(url)
    if cp.exists():
        try:
            with open(cp, "r", encoding="utf-8") as f:
                return json.load(f)
        except (json.JSONDecodeError, OSError):
            cp.unlink(missing_ok=True)  # caché corrupta: re-descarga

    if offline:
        raise FileNotFoundError(f"offline y sin caché para {url}")

    last_err = None
    for attempt in range(MAX_RETRIES):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=15) as r:
                payload = r.read()
            data = json.loads(payload)
            with open(cp, "w", encoding="utf-8") as f:
                f.write(payload.decode("utf-8"))
            return data
        except Exception as e:  # noqa: BLE001
            last_err = e
            time.sleep(RETRY_DELAY * (attempt + 1))
    raise RuntimeError(f"no se pudo obtener {url}: {last_err}")


def name_to_id_map() -> dict:
    """nombre (minúsculas) -> national id desde pokemon.json (cubre 1..1025)."""
    m = {}
    if SPECIES_JSON.exists():
        with open(SPECIES_JSON, encoding="utf-8") as f:
            for p in json.load(f):
                m[str(p["name"]).lower()] = int(p["id"])
    return m


def resolve_species_id(name: str, n2id: dict, offline: bool) -> int:
    """Resuelve el ID nacional de una especie por su nombre PokéAPI (minúsculas)."""
    key = str(name).lower()
    if key in n2id:
        return int(n2id[key])
    sp = fetch_json(f"{API_BASE}/pokemon-species/{name}", offline)
    if sp.get("id") is not None:
        return int(sp["id"])
    raise KeyError(f"no se pudo resolver ID para {name}")


def introduced_generation(species: dict) -> int:
    """Generación de introducción (1..9) desde /pokemon-species."""
    gen = species.get("generation", {})
    name = gen.get("name", "") if isinstance(gen, dict) else str(gen)
    m = re.search(r"generation-([ivxlcdm]+)$", name, re.I)
    if not m:
        return 1
    roman = m.group(1).lower()
    vals = {"i": 1, "v": 5, "x": 10, "l": 50, "c": 100, "d": 500, "m": 1000}
    total, prev = 0, 0
    for ch in reversed(roman):
        v = vals[ch]
        total += -v if v < prev else v
        prev = v
    return max(1, min(9, total))


# ----------------------------------------------------------------------------
# Nombres ES + tipo de movimientos (caché por movimiento)
# ----------------------------------------------------------------------------
def _clean_move_text(text: str) -> str:
    return re.sub(r"\s+", " ", (text or "").replace("\f", " ").replace("\n", " ")).strip()


def move_details(move_id: int, offline: bool):
    """Nombre/tipo/datos de combate/descripción, priorizando siempre español."""
    mv = fetch_json(f"{API_BASE}/move/{move_id}", offline)
    type_code = TYPE_MAP.get((mv.get("type") or {}).get("name", ""), 0)

    names = {n["language"]["name"]: n["name"] for n in mv.get("names", []) if n.get("name")}
    es = names.get("es")
    name_en = not bool(es)
    name = es or names.get("en") or mv.get("name", "") or f"move-{move_id}"
    descriptions = {}
    for entry in mv.get("flavor_text_entries", []):
        lang = (entry.get("language") or {}).get("name")
        text = _clean_move_text(entry.get("flavor_text", ""))
        if lang and text:
            descriptions[lang] = text
    if not descriptions.get("es"):
        for entry in mv.get("effect_entries", []):
            lang = (entry.get("language") or {}).get("name")
            text = _clean_move_text(entry.get("short_effect") or entry.get("effect", ""))
            if lang and text:
                descriptions[lang] = text
    local_es = MOVE_TRANSLATIONS.get(str(move_id))
    desc_en = not bool(descriptions.get("es") or local_es)
    description = descriptions.get("es") or local_es or descriptions.get("en") or "Sin descripción disponible."
    category = {"physical": 1, "special": 2, "status": 3}.get(
        (mv.get("damage_class") or {}).get("name", ""), 0)
    norm = lambda value: 255 if value is None else min(int(value), 254)
    return (name, type_code, norm(mv.get("power")), norm(mv.get("accuracy")),
            norm(mv.get("pp")), category, description, name_en, desc_en)


# ----------------------------------------------------------------------------
# Construcción del árbol evolutivo
# ----------------------------------------------------------------------------
def _condition_from_detail(detail: dict, dest_gen: int) -> tuple[int, int, str]:
    """Convierte un evolution_detail de PokéAPI en (kind, value_u16, text_es)."""
    trigger = (detail.get("trigger") or {}).get("name", "")
    # Nivel
    lvl = detail.get("min_level")
    if lvl:
        return COND_LEVEL, int(lvl), f"Sube al nivel {int(lvl)}"

    # Interambio con especie concreta
    trade = detail.get("trade_species")
    if trigger == "trade" or trade:
        return COND_TRADE, 0, "Intercambio"

    # Amistad / felicidad
    hap = detail.get("min_happiness")
    aff = detail.get("min_affection")
    tod = detail.get("time_of_day")
    if (hap is not None or aff is not None) and tod == "day":
        return COND_FRIENDSHIP_DAY, int(hap or aff or 0), "Amistad durante el día"
    if (hap is not None or aff is not None) and tod == "night":
        return COND_FRIENDSHIP_NIGHT, int(hap or aff or 0), "Amistad durante la noche"
    if hap is not None and int(hap) >= 160:
        return COND_AFFECTION, 160, "Con mucho afecto"
    if hap is not None:
        return COND_FRIENDSHIP, int(hap), "Con alta amistad"
    if aff is not None:
        return COND_AFFECTION, int(aff), f"Afecto {int(aff)}"

    # Objeto (use-item trigger → field "item" = dict or string)
    item = detail.get("item")
    if item:
        name = item.get("name", "") if isinstance(item, dict) else str(item)
        es = ITEM_ES.get(name, name.replace("-", " ").title())
        return COND_ITEM, 0, es
    # Objeto equipado (level-up with held item → field "held_item")
    held = detail.get("held_item")
    if held:
        name = held.get("name", "") if isinstance(held, dict) else str(held)
        es = ITEM_ES.get(name, name.replace("-", " ").title())
        return COND_ITEM, 0, es

    # Hora del día
    if tod:
        return COND_TIME, 1 if tod == "day" else 2 if tod == "night" else 0, \
               "Durante el día" if tod == "day" else "Durante la noche" if tod == "night" else "A una hora concreta"

    # Género
    g = detail.get("gender")
    if g:
        return COND_GENDER, 0, "Según el género"

    # Movimiento conocido
    hm = detail.get("holder_move")
    if hm:
        return COND_MOVE_KNOWN, 0, "Sabiendo un movimiento"

    # Tipo conocido
    hmt = detail.get("holder_move_type")
    if hmt:
        return COND_TYPE_KNOWN, 0, "Sabiendo un tipo de movimiento"

    # Especie/tipo en el equipo
    ps = detail.get("party_species")
    pst = detail.get("party_species_type")
    if ps or pst:
        return COND_PARTY, 0, "Con cierta especie en el equipo"

    # Lugar
    loc = detail.get("location")
    if loc:
        return COND_LOCATION, 0, "En un lugar concreto"

    # Belleza
    beauty = detail.get("min_beauty")
    if beauty is not None:
        return COND_BEAUTY, int(beauty), f"Belleza {int(beauty)}"

    # Clima / objeto equipado (no expuestos por PokéAPI de forma fiable)
    return COND_SPECIAL, 0, "Condición especial"


def build_evolution_tree(pokemon_id: int, n2id: dict, offline: bool) -> list[dict]:
    """
    Construye la cadena evolutiva COMPLETA (raíz -> ... -> ramas) como lista de
    nodos en orden pre-orden. Cada nodo: {species_id, introduced_generation,
    parent_index, conditions:[(kind, value, text)]}.
    Detecta ciclos por visita.
    """
    # 1) Obtener la URL de la cadena desde /pokemon-species
    sp = fetch_json(f"{API_BASE}/pokemon-species/{pokemon_id}", offline)
    chain_url = (sp.get("evolution_chain") or {}).get("url")
    if not chain_url:
        return []  # sin cadena evolutiva: nodo único implícito lo gestiona el writer

    chain = fetch_json(chain_url, offline)
    root_chain = chain.get("chain") or {}

    nodes: list[dict] = []
    seen_species = set()

    def add_species(name: str) -> int:
        """Devuelve el índice del nodo de esta especie (o -1 si no resuelve)."""
        sid = resolve_species_id(name, n2id, offline)
        if not (MIN_SPECIES_ID <= sid <= MAX_SPECIES_ID):
            return -1
        if sid in seen_species:
            for i, n in enumerate(nodes):
                if n["species_id"] == sid:
                    return i
            return -1
        seen_species.add(sid)
        sp2 = fetch_json(f"{API_BASE}/pokemon-species/{sid}", offline)
        nodes.append({
            "species_id": sid,
            "introduced_generation": introduced_generation(sp2),
            "parent_index": -1,
            "conditions": [],
        })
        return len(nodes) - 1

    def walk(chain_node: dict, parent_index: int):
        name = (chain_node.get("species") or {}).get("name")
        if not name:
            return
        idx = add_species(name)
        if idx < 0:
            return
        nodes[idx]["parent_index"] = parent_index
        # condiciones = evolution_details de ESTE nodo (cómo se llega a él desde el padre)
        if parent_index >= 0:
            dest_gen = nodes[idx]["introduced_generation"]
            for det in chain_node.get("evolution_details", []):
                kind, value, text = _condition_from_detail(det, dest_gen)
                nodes[idx]["conditions"].append((kind, min(value, 0xFFFF), text))
        for child in chain_node.get("evolves_to", []):
            walk(child, idx)

    walk(root_chain, -1)

    if len(nodes) > MAX_NODES:
        raise ValueError(f"cadena evolutiva demasiado larga ({len(nodes)} > {MAX_NODES})")
    return nodes


# ----------------------------------------------------------------------------
# Movimientos level-up por generación canónica
# ----------------------------------------------------------------------------
def _move_id_from_mv(mv: dict) -> int | None:
    """Extrae el ID del movimiento. En /pokemon/{id} el objeto move es {name,url}."""
    m = mv.get("move") or {}
    if m.get("id") is not None:
        return int(m["id"])
    mu = re.search(r"/move/(\d+)/?$", m.get("url", ""))
    return int(mu.group(1)) if mu else None


def extract_level_moves(pokemon_id: int, offline: bool) -> dict[str, list[tuple[int, int, int]]]:
    """
    Devuelve {version_group_name: [(level, order, move_id), ...]} solo con
    move_learn_method == 'level-up'. `order` preserva el orden de la fuente
    dentro de cada nivel (para dedup y ordenación estable).
    """
    pk = fetch_json(f"{API_BASE}/pokemon/{pokemon_id}", offline)
    per_vgroup: dict[str, dict[int, list[tuple[int, int, int]]]] = {}
    order = 0
    for mv in pk.get("moves", []):
        for vgd in mv.get("version_group_details", []):
            method = (vgd.get("move_learn_method") or {}).get("name")
            if method != "level-up":
                continue
            vg = (vgd.get("version_group") or {}).get("name")
            if not vg:
                continue
            level = int(vgd.get("level_learned_at", 0) or 0)
            move_id = _move_id_from_mv(mv)
            if move_id is None:
                continue
            per_vgroup.setdefault(vg, {}).setdefault(level, []).append((level, order, int(move_id)))
            order += 1

    result: dict[str, list[tuple[int, int, int]]] = {}
    for vg, by_level in per_vgroup.items():
        moves = []
        for level in sorted(by_level):
            moves.extend(sorted(by_level[level], key=lambda t: t[1]))
        result[vg] = moves
    return result


# ----------------------------------------------------------------------------
# Serialización binaria
# ----------------------------------------------------------------------------
def _encode_text(s: str, limit: int = MAX_NAME_BYTES) -> bytes:
    b = s.encode("utf-8")
    if len(b) > limit:
        b = b[:limit]
        while b:
            try:
                b.decode("utf-8")
                break
            except UnicodeDecodeError:
                b = b[:-1]
    return b


def serialize_pkme(pokemon_id: int, nodes: list[dict],
                   generations: dict[int, list[tuple]]) -> bytes:
    """
    generations: {generation(1..9): [(level, order, move_id, type_code, flags, name), ...]}

    Orden de campos EXACTO según DATOS_MECANICAS_SD.md (little-endian):
      nodo:        species_id u16, introduced_generation u8, parent_index i8, condition_count u8
      condición:   kind u8, introduced_generation u8, value_u16 u16, text_len u8, text_utf8
      sección:     generation u8, version_group_code u8, move_count u16
      movimiento:  level u8, order u16, move_id u16, type_code u8, flags u8, name_len u8, name_utf8
    """
    if len(nodes) > MAX_NODES:
        raise ValueError(f"más de {MAX_NODES} nodos evolutivos")

    payload = b""

    # --- Cadena evolutiva común ---
    payload += struct.pack("<B", len(nodes))
    for n in nodes:
        conds = n.get("conditions", [])
        payload += struct.pack(
            "<HBbB",
            n["species_id"] & 0xFFFF,
            n["introduced_generation"] & 0xFF,
            n["parent_index"],
            len(conds) & 0xFF,
        )
        for kind, value, text in conds:
            tb = _encode_text(text)
            payload += struct.pack(
                "<BBHB",
                kind & 0xFF,
                n["introduced_generation"] & 0xFF,  # intro gen de la condición = gen destino
                min(int(value), 0xFFFF),
                len(tb) & 0xFF,
            )
            payload += tb

    # --- Secciones por generación (ascendente) ---
    gen_list = sorted(generations.keys())
    for g in gen_list:
        moves = generations[g]
        if len(moves) > MAX_MOVES_PER_GEN:
            raise ValueError(f"más de {MAX_MOVES_PER_GEN} movimientos en gen {g}")
        payload += struct.pack(
            "<BBH",
            g & 0xFF,
            VGROUP_CODE.get(GEN_VGROUP.get(g), g & 0xFF),
            len(moves) & 0xFFFF,
        )
        for (level, order, move_id, type_code, flags, name,
             power, accuracy, pp, category, description) in moves:
            nb = _encode_text(name)
            db = _encode_text(description, MAX_MOVE_DESCRIPTION_BYTES)
            payload += struct.pack(
                "<BHHBBB",  # level u8, order u16, move_id u16, type u8, flags u8, name_len u8
                level & 0xFF, order & 0xFFFF, move_id & 0xFFFF,
                type_code & 0xFF, flags & 0xFF, len(nb) & 0xFF,
            )
            payload += nb
            payload += struct.pack("<BBBBB", power, accuracy, pp, category, len(db))
            payload += db

    # --- Cabecera ---
    mask = 0
    for g in gen_list:
        mask |= 1 << (g - 1)
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    # Cabecera (20 B): magic 4s, version H, pokemon_id H, payload_size I, crc I,
    # generation_mask H, generation_count B, reserved B
    header = struct.pack(
        "<4sHHIIHBB",
        PKME_MAGIC, PKME_VERSION, pokemon_id,
        len(payload), crc,
        mask & 0xFFFF, len(gen_list) & 0xFF, 0,
    )
    return header + payload


def parse_pkme(data: bytes) -> dict:
    """Parsea y valida un binario PKME (equivalente Python del firmware)."""
    out = {"valid": False}
    if len(data) < 20 or data[:4] != PKME_MAGIC:
        return out
    (version, pid, payload_size, crc, mask, gcount, _res) = struct.unpack_from(
        "<4sHHIIHBB", data, 0)[1:]
    if version != PKME_VERSION:
        return out
    payload = data[20:]
    if len(payload) != payload_size:
        return out
    if (zlib.crc32(payload) & 0xFFFFFFFF) != crc:
        return out
    out.update(valid=True, id=pid, mask=mask, generation_count=gcount)

    off = 0
    node_count = payload[off]; off += 1
    nodes = []
    for _ in range(node_count):
        sid, igen, pidx, ccount = struct.unpack_from("<HBbB", payload, off); off += 5
        conds = []
        for _ in range(ccount):
            kind, cigen, cval, tlen = struct.unpack_from("<BBHB", payload, off); off += 5
            text = payload[off:off + tlen].decode("utf-8", "replace"); off += tlen
            conds.append((kind, cval, text))
        nodes.append((sid, igen, pidx, conds))
    out["nodes"] = nodes

    sections = []
    for _ in range(gcount):
        gen, vcode, mcount = struct.unpack_from("<BBH", payload, off); off += 4
        moves = []
        for _ in range(mcount):
            level, order, mid, tcode, flags, nlen = struct.unpack_from("<BHHBBB", payload, off); off += 8
            name = payload[off:off + nlen].decode("utf-8", "replace"); off += nlen
            power, accuracy, pp, category, dlen = struct.unpack_from("<BBBBB", payload, off); off += 5
            description = payload[off:off + dlen].decode("utf-8", "replace"); off += dlen
            moves.append((level, order, mid, tcode, flags, name,
                          power, accuracy, pp, category, description))
        sections.append((gen, vcode, moves))
    out["sections"] = sections
    return out


# ----------------------------------------------------------------------------
# Generación de una especie
# ----------------------------------------------------------------------------
def build_species(pokemon_id: int, offline: bool) -> tuple[bytes, dict]:
    n2id = name_to_id_map()

    # Árbol evolutivo
    nodes = build_evolution_tree(pokemon_id, n2id, offline)

    # Movimientos por generación canónica
    per_vgroup = extract_level_moves(pokemon_id, offline)
    generations = {}
    for g, vg_name in GEN_VGROUP.items():
        moves = per_vgroup.get(vg_name)
        if not moves:
            continue
        enriched = []
        for (level, order, move_id) in moves:
            name, type_code, power, accuracy, pp, category, description, name_en, desc_en = move_details(move_id, offline)
            flags = (MOVE_FLAG_EN_NAME if name_en else 0) | (MOVE_FLAG_EN_DESCRIPTION if desc_en else 0)
            enriched.append((level, order, move_id, type_code, flags, name,
                             power, accuracy, pp, category, description))
        generations[g] = enriched

    blob = serialize_pkme(pokemon_id, nodes, generations)
    meta = {
        "id": pokemon_id,
        "node_count": len(nodes),
        "generation_count": len(generations),
        "generations": sorted(generations.keys()),
        "moves_per_gen": {g: len(v) for g, v in generations.items()},
    }
    return blob, meta


def write_species(output_dir: Path, blob: bytes, meta: dict):
    data_dir = output_dir / "data"
    data_dir.mkdir(parents=True, exist_ok=True)
    path = data_dir / f"{meta['id']:04d}.bin"
    staging = path.with_suffix(".tmp")
    with open(staging, "wb") as f:
        f.write(blob)
    # releer y validar CRC antes de renombrar (escritura atómica)
    with open(staging, "rb") as f:
        check = parse_pkme(f.read())
    if not check["valid"]:
        staging.unlink(missing_ok=True)
        raise RuntimeError(f"validación de relectura falló para {meta['id']}")
    staging.replace(path)
    return path


def summarize_blob(blob: bytes) -> dict | None:
    """Extrae (id, nodos, generaciones, mov/gen) de un binario ya escrito."""
    pr = parse_pkme(blob)
    if not pr["valid"]:
        return None
    moves_per_gen = {gen: len(moves) for (gen, _vc, moves) in pr["sections"]}
    return {
        "id": pr["id"],
        "node_count": len(pr["nodes"]),
        "generation_count": len(pr["sections"]),
        "generations": sorted(moves_per_gen.keys()),
        "moves_per_gen": moves_per_gen,
    }


def generate_manifest(output_dir: Path, metas: list[dict], errors: list[dict]):
    # El manifiesto refleja TODOS los .bin presentes en data/ (coherente con disco),
    # no solo el lote de esta ejecución. Así las ejecuciones por lotes no pierden especies.
    data_dir = output_dir / "data"
    species: list[dict] = []
    if data_dir.exists():
        for f in sorted(data_dir.glob("*.bin")):
            s = summarize_blob(f.read_bytes())
            if s:
                species.append(s)
    species.sort(key=lambda x: x["id"])

    manifest = {
        "format": "PKME",
        "version": PKME_VERSION,
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "generator": "generate_mechanics.py",
        "total_species": len(species),
        "data_dir": "data/",
        "generation_table": {
            str(g): {"version_group": name, "code": code}
            for g, name, code in sorted(
                ((g, name, VGROUP_CODE[name]) for g, name in GEN_VGROUP.items())
            )
        },
        "condition_kinds": {
            "special": COND_SPECIAL, "level": COND_LEVEL, "item": COND_ITEM,
            "trade": COND_TRADE, "friendship": COND_FRIENDSHIP, "time": COND_TIME,
            "gender": COND_GENDER, "move_known": COND_MOVE_KNOWN,
            "type_known": COND_TYPE_KNOWN, "location": COND_LOCATION,
            "weather": COND_WEATHER, "party": COND_PARTY, "equip": COND_EQUIP,
            "beauty": COND_BEAUTY, "affection": COND_AFFECTION,
            "friendship_day": COND_FRIENDSHIP_DAY,
            "friendship_night": COND_FRIENDSHIP_NIGHT,
        },
        "totals": {
            "nodes": sum(m["node_count"] for m in species),
            "sections": sum(m["generation_count"] for m in species),
            "moves": sum(sum(m["moves_per_gen"].values()) for m in species),
        },
        "errors": errors,
        "species": [
            {"id": m["id"], "nodes": m["node_count"], "generations": m["generations"],
             "moves_per_gen": m["moves_per_gen"]}
            for m in species
        ],
    }
    output_dir.mkdir(parents=True, exist_ok=True)
    with open(output_dir / "manifest.json", "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    return manifest


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--first", type=int, default=None, help="ID inicial (rango)")
    ap.add_argument("--last", type=int, default=None, help="ID final (rango)")
    ap.add_argument("--ids", type=str, default=None, help="lista de IDs separados por coma")
    ap.add_argument("--output", type=Path, default=OUTPUT_DIR, help="directorio de salida")
    ap.add_argument("--offline", action="store_true", help="solo usar caché (sin red)")
    args = ap.parse_args()

    if args.ids:
        ids = sorted({int(x) for x in args.ids.split(",") if x.strip()})
    elif args.first is not None and args.last is not None:
        ids = list(range(args.first, args.last + 1))
    else:
        ap.error("indica --ids o --first/--last")

    ids = [i for i in ids if MIN_SPECIES_ID <= i <= MAX_SPECIES_ID]
    if not ids:
        print("no hay IDs válidos en el rango"); sys.exit(1)

    print(f"PKME generator: {len(ids)} especies  (offline={args.offline})")
    print(f"Salida: {args.output}")

    metas, errors = [], []
    for i, pid in enumerate(ids, 1):
        try:
            blob, meta = build_species(pid, args.offline)
            path = write_species(args.output, blob, meta)
            metas.append(meta)
            gens = ",".join(str(g) for g in meta["generations"])
            print(f"  [{i}/{len(ids)}] {pid:04d}  nodos={meta['node_count']} "
                  f"gens=[{gens}] mov={meta['moves_per_gen']}")
        except Exception as e:  # noqa: BLE001
            msg = f"{type(e).__name__}: {e}"
            errors.append({"id": pid, "error": msg})
            print(f"  [{i}/{len(ids)}] {pid:04d}  ERROR {msg}")

    manifest = generate_manifest(args.output, metas, errors)

    print()
    print(f"OK: {len(metas)} especies, {len(errors)} errores")
    print(f"  total nodos={manifest['totals']['nodes']} secciones={manifest['totals']['sections']} "
          f"movimientos={manifest['totals']['moves']}")
    if errors:
        print(f"  ERROR (una generación incompleta hace fallar el dataset):")
        for e in errors[:20]:
            print(f"    {e['id']:04d}: {e['error']}")
        sys.exit(1)


if __name__ == "__main__":
    main()
