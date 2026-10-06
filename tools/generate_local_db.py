#!/usr/bin/env python3
"""
Genera la base de datos local para el Pokédex ESP32-S3.

Output:
  sd_dataset/
    pokemon_index.bin    - Índice compacto (cargado en PSRAM)
    data/NNNN.bin       - Detalle por especie
    manifest.json       - Metadata para firmware
    sprites/...         - (ya existen, no se modifican)

Formato pokemon_index.bin:
  Header (12 bytes):
    magic: "PKDX" (4)
    version: uint16 (2) = 1
    count: uint16 (2)
    crc32: uint32 (4)

  Entry (26 bytes each):
    id: uint16 (2)
    name: char[16] (16) — display name, max 15 chars + null
    type1: uint8 (1) — 1-18
    type2: uint8 (1) — 1-18 or 0
    generation: uint8 (1) — 1-9
    height_cm: uint16 (2)
    weight_dkg: uint16 (2) — 0.1 kg units
    base_exp: uint8 (1)

Formato data/NNNN.bin:
  Header (8 bytes):
    magic: "PKDP" (4)
    version: uint16 (2)
    id: uint16 (2)
  Body:
    name: char[20]
    name_es: char[20]
    type1: uint8
    type2: uint8
    generation: uint8
    height_cm: uint16
    weight_dkg: uint16
    base_exp: uint16
    hp, atk, def, spa, spd, spe: 6 x uint8
    desc_en_len: uint8
    desc_en: char[desc_en_len]
    desc_es_len: uint8
    desc_es: char[desc_es_len]
"""

import json
import struct
import os
import sys
import zlib
from pathlib import Path

# Type mapping: name -> uint8 code
TYPE_MAP = {
    "normal": 1, "fire": 2, "water": 3, "electric": 4,
    "grass": 5, "ice": 6, "fighting": 7, "poison": 8,
    "ground": 9, "flying": 10, "psychic": 11, "bug": 12,
    "rock": 13, "ghost": 14, "dragon": 15, "dark": 16,
    "steel": 17, "fairy": 18
}

# Display names for species where the dash is part of the name
# (not a form suffix). These get special treatment.
SPECIAL_NAMES = {
    29: "Nidoran-F",
    32: "Nidoran-M",
    122: "Mr. Mime",
    250: "Ho-oh",
    439: "Mime Jr.",
    474: "Porygon-Z",
    782: "Jangmo-o",
    783: "Hakamo-o",
    784: "Kommo-o",
    785: "Tapu Koko",
    786: "Tapu Lele",
    787: "Tapu Bulu",
    788: "Tapu Fini",
    984: "Great Tusk",
    985: "Scream Tail",
    986: "Brute Bonnet",
    987: "Flutter Mane",
    988: "Slither Wing",
    989: "Sandy Shocks",
    990: "Iron Treads",
    991: "Iron Bundle",
    992: "Iron Hands",
    993: "Iron Jugulis",
    994: "Iron Moth",
    995: "Iron Thorns",
    1001: "Wo-Chien",
    1002: "Chien-Pao",
    1003: "Ting-Lu",
    1004: "Chi-Yu",
    1005: "Roaring Moon",
    1006: "Iron Valiant",
    1009: "Walking Wake",
    1010: "Iron Leaves",
    1020: "Gouging Fire",
    1021: "Raging Bolt",
    1022: "Iron Boulder",
    1023: "Iron Crown",
}

def get_display_name(pokemon_id: int, api_name: str) -> str:
    """Get the display name for a species."""
    if pokemon_id in SPECIAL_NAMES:
        return SPECIAL_NAMES[pokemon_id]
    # Strip form suffix (everything after first dash)
    if '-' in api_name:
        base = api_name.split('-')[0]
        # Capitalize properly
        return base[0].upper() + base[1:]
    return api_name[0].upper() + api_name[1:]


def type_to_code(type_name: str) -> int:
    return TYPE_MAP.get(type_name.lower(), 0)


def parse_generation(val) -> int:
    """Parse generation from 'generation-i' or int."""
    if isinstance(val, int):
        return val
    if isinstance(val, str):
        # e.g., 'generation-i', 'generation-viii'
        import re
        m = re.search(r'[ivxlcdm]+$', val.lower())
        if m:
            roman = m.group(0)
            roman_map = {'i':1,'v':5,'x':10,'l':50,'c':100,'d':500,'m':1000}
            result = 0
            for i, ch in enumerate(roman):
                val_ = roman_map[ch]
                if i + 1 < len(roman) and roman_map[roman[i+1]] > val_:
                    result -= val_
                else:
                    result += val_
            return result
    return 1


def encode_name_utf8(name: str, max_bytes: int) -> bytes:
    """Encode name as UTF-8, padded to exactly max_bytes."""
    encoded = name.encode('utf-8')
    if len(encoded) >= max_bytes:
        # Truncate to fit null terminator
        encoded = encoded[:max_bytes - 1]
        # Ensure valid UTF-8 (don't split multi-byte chars)
        while encoded:
            try:
                encoded.decode('utf-8')
                break
            except UnicodeDecodeError:
                encoded = encoded[:-1]
    # Pad to exactly max_bytes
    encoded += b'\x00' * (max_bytes - len(encoded))
    return encoded[:max_bytes]


def generate_index(pokemons: list, output_path: Path):
    """Generate pokemon_index.bin"""
    entries = b''
    
    for p in pokemons:
        pid = p['id']
        name = get_display_name(pid, p['name'])
        name_bytes = encode_name_utf8(name, 16)
        
        types = p.get('types', [])
        type1 = type_to_code(types[0]) if len(types) > 0 else 0
        type2 = type_to_code(types[1]) if len(types) > 1 else 0
        
        gen = parse_generation(p.get('generation', 1))
        height_cm = int(p.get('height', 0) * 100)  # m -> cm
        weight_dkg = int(p.get('weight', 0) * 10)  # kg -> 0.1kg
        base_exp = min(int(p.get('base_experience', 0)), 255)
        
        entry = struct.pack('<H16sBBBHHB',
                           pid,
                           name_bytes,
                           type1,
                           type2,
                           gen,
                           height_cm,
                           weight_dkg,
                           base_exp
                           )
        entries += entry
    
    # Header
    count = len(pokemons)
    crc = zlib.crc32(entries) & 0xFFFFFFFF
    header = struct.pack('<4sHHI', b'PKDX', 1, count, crc)
    
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(entries)
    
    size = os.path.getsize(output_path)
    print(f"  Index: {count} entries, {size} bytes")
    return count


def generate_detail(pokemon: dict, output_path: Path):
    """Generate data/NNNN.bin for one pokemon."""
    pid = pokemon['id']
    name = get_display_name(pid, pokemon['name'])
    name_es = pokemon.get('name_es', name)
    
    types = pokemon.get('types', [])
    type1 = type_to_code(types[0]) if len(types) > 0 else 0
    type2 = type_to_code(types[1]) if len(types) > 1 else 0
    
    gen = parse_generation(pokemon.get('generation', 1))
    height_cm = int(pokemon.get('height', 0) * 100)
    weight_dkg = int(pokemon.get('weight', 0) * 10)
    base_exp = min(int(pokemon.get('base_experience', 0)), 65535)
    
    stats = pokemon.get('stats', {})
    hp = min(int(stats.get('hp', 0)), 255)
    atk = min(int(stats.get('attack', 0)), 255)
    dfn = min(int(stats.get('defense', 0)), 255)
    spa = min(int(stats.get('special_attack', 0)), 255)
    spd = min(int(stats.get('special_defense', 0)), 255)
    spe = min(int(stats.get('speed', 0)), 255)
    
    desc_en = (pokemon.get('description_en') or '')[:254]
    desc_es = (pokemon.get('description_es') or '')[:254]
    
    desc_en_bytes = desc_en.encode('utf-8')
    desc_es_bytes = desc_es.encode('utf-8')
    
    # Pack: header + fixed body + variable descriptions
    body = struct.pack('<4sHH', b'PKDP', 1, pid)  # 8 bytes header
    
    name_bytes = encode_name_utf8(name, 20)
    name_es_bytes = encode_name_utf8(name_es, 20)
    
    body += name_bytes
    body += name_es_bytes
    body += struct.pack('<BBB', type1, type2, gen)
    body += struct.pack('<HH', height_cm, weight_dkg)
    body += struct.pack('<H', base_exp)
    body += struct.pack('<6B', hp, atk, dfn, spa, spd, spe)
    body += struct.pack('<B', len(desc_en_bytes))
    body += desc_en_bytes
    body += struct.pack('<B', len(desc_es_bytes))
    body += desc_es_bytes
    
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'wb') as f:
        f.write(body)
    
    return os.path.getsize(output_path)


def generate_manifest(pokemons: list, output_path: Path):
    """Generate manifest.json"""
    manifest = {
        "format_version": 1,
        "total": len(pokemons),
        "index_file": "pokemon_index.bin",
        "data_dir": "data/",
        "sprites": {
            "small_dir": "sprites/small/",
            "large_dir": "sprites/large/",
            "format": "r565"
        },
        "index_entry_size": 26,
        "index_header_size": 12,
        "index_total_bytes": 12 + 26 * len(pokemons),
        "types": {v: k for k, v in TYPE_MAP.items()},
        "sample": {
            "first": pokemons[0]['id'],
            "last": pokemons[-1]['id']
        }
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
    print(f"  Manifest: {os.path.getsize(output_path)} bytes")


def main():
    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    data_file = project_root / 'backend' / 'app' / 'data' / 'pokemon.json'
    output_dir = project_root / 'sd_dataset'
    
    if not data_file.exists():
        print(f"ERROR: {data_file} not found")
        sys.exit(1)
    
    print(f"Reading: {data_file}")
    with open(data_file, encoding='utf-8') as f:
        pokemons = json.load(f)
    
    print(f"Total species: {len(pokemons)}")
    print(f"Output: {output_dir}")
    print()
    
    # Generate index
    print("Generating index...")
    count = generate_index(pokemons, output_dir / 'pokemon_index.bin')
    
    # Generate detail files
    print("Generating detail files...")
    data_dir = output_dir / 'data'
    total_bytes = 0
    for p in pokemons:
        path = data_dir / f"{p['id']:04d}.bin"
        size = generate_detail(p, path)
        total_bytes += size
    print(f"  {count} files, {total_bytes} bytes total")
    
    # Generate manifest
    print("Generating manifest...")
    generate_manifest(pokemons, output_dir / 'manifest.json')
    
    print()
    print("Done! Copy sd_dataset/ to SD card /pokedex/pokemon/ to use.")
    print(f"  SD path: /pokedex/pokemon/pokemon_index.bin")
    print(f"  SD path: /pokedex/pokemon/data/NNNN.bin")


if __name__ == '__main__':
    main()
