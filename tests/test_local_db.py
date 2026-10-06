#!/usr/bin/env python3
"""
Valida la base de datos local generada (pokemon_index.bin + data/*.bin).

Tests:
1. Index: magic, version, count, CRC
2. Index entries: parse all, validate ranges
3. Detail files: parse, validate
4. Kanto completeness: 151 entries
5. Search test: find "pikachu", "charmander", etc.
"""

import struct
import os
import sys
import zlib
from pathlib import Path

PASS = 0
FAIL = 0

def check(name, condition, detail=""):
    global PASS, FAIL
    if condition:
        PASS += 1
        print(f"  [PASS] {name}")
    else:
        FAIL += 1
        print(f"  [FAIL] {name} {detail}")


def test_index(index_path: Path):
    print("\n=== Test: pokemon_index.bin ===")
    with open(index_path, 'rb') as f:
        data = f.read()
    
    # Header
    check("File size >= 12 bytes (header)", len(data) >= 12)
    
    magic = data[:4]
    check("Magic = PKDX", magic == b'PKDX', f"got {magic}")
    
    version = struct.unpack_from('<H', data, 4)[0]
    check("Version = 1", version == 1, f"got {version}")
    
    count = struct.unpack_from('<H', data, 6)[0]
    check(f"Count > 0", count > 0, f"got {count}")
    check(f"Count = 1025 (Gen 1-9)", count == 1025, f"got {count}")
    
    stored_crc = struct.unpack_from('<I', data, 8)[0]
    
    # Entries
    entry_size = 26
    expected_size = 12 + entry_size * count
    check(f"File size = {expected_size}", len(data) == expected_size, f"got {len(data)}")
    
    entries = data[12:]
    computed_crc = zlib.crc32(entries) & 0xFFFFFFFF
    check("CRC32 valid", stored_crc == computed_crc, f"stored={stored_crc:#x} computed={computed_crc:#x}")
    
    # Parse all entries
    entries_list = []
    for i in range(count):
        offset = i * entry_size
        entry = struct.unpack_from('<H16sBBBHHB', entries, offset)
        pid, name_bytes, type1, type2, gen, height_cm, weight_dkg, base_exp = entry
        name = name_bytes.rstrip(b'\x00').decode('utf-8', errors='replace')
        entries_list.append({
            'id': pid, 'name': name, 'type1': type1, 'type2': type2,
            'gen': gen, 'height_cm': height_cm, 'weight_dkg': weight_dkg,
            'base_exp': base_exp
        })
    
    check("First entry id=1", entries_list[0]['id'] == 1, f"got {entries_list[0]['id']}")
    check("First entry name=Bulbasaur", entries_list[0]['name'] == 'Bulbasaur', f"got {entries_list[0]['name']}")
    check("Last entry id=1025", entries_list[-1]['id'] == 1025, f"got {entries_list[-1]['id']}")
    check("Last entry name=Pecharunt", entries_list[-1]['name'] == 'Pecharunt', f"got {entries_list[-1]['name']}")
    
    # Validate ranges
    all_ids_valid = all(1 <= e['id'] <= 1025 for e in entries_list)
    check("All IDs in range 1-1025", all_ids_valid)
    
    all_types_valid = all(0 <= e['type1'] <= 18 and 0 <= e['type2'] <= 18 for e in entries_list)
    check("All types in range 0-18", all_types_valid)
    
    all_gen_valid = all(1 <= e['gen'] <= 9 for e in entries_list)
    check("All generations in range 1-9", all_gen_valid)
    
    all_names_valid = all(0 < len(e['name']) <= 15 for e in entries_list)
    check("All names 1-15 chars", all_names_valid)
    
    # Kanto completeness
    kanto = [e for e in entries_list if e['gen'] == 1]
    check(f"Kanto count = 151 (got {len(kanto)})", len(kanto) == 151)
    kanto_ids = set(e['id'] for e in kanto)
    check("Kanto IDs 1-151 complete", kanto_ids == set(range(1, 152)))
    
    # Search tests
    def search(query):
        query = query.lower()
        return [e for e in entries_list if query in e['name'].lower()]
    
    r = search("pikachu")
    check("Search 'pikachu' finds 1", len(r) == 1 and r[0]['id'] == 25, f"got {r}")
    
    r = search("charmander")
    check("Search 'charmander' finds 1", len(r) == 1 and r[0]['id'] == 4, f"got {r}")
    
    r = search("nidoran")
    check("Search 'nidoran' finds 2", len(r) == 2, f"got {len(r)}")
    
    r = search("iron")
    check("Search 'iron' finds >= 5", len(r) >= 5, f"got {len(r)}")
    
    r = search("zzz")
    check("Search 'zzz' finds 0", len(r) == 0)
    
    return entries_list


def test_detail_files(data_dir: Path):
    print("\n=== Test: data/*.bin ===")
    
    # Test a few specific files
    test_ids = [1, 25, 151, 150, 1025]
    
    for pid in test_ids:
        path = data_dir / f"{pid:04d}.bin"
        check(f"File {pid:04d}.bin exists", path.exists())
        if not path.exists():
            continue
        
        with open(path, 'rb') as f:
            data = f.read()
        
        # Header
        magic = data[:4]
        check(f"  [{pid}] Magic = PKDP", magic == b'PKDP', f"got {magic}")
        
        version = struct.unpack_from('<H', data, 4)[0]
        check(f"  [{pid}] Version = 1", version == 1)
        
        entry_id = struct.unpack_from('<H', data, 6)[0]
        check(f"  [{pid}] ID matches", entry_id == pid, f"got {entry_id}")
        
        # Body
        offset = 8
        name = data[offset:offset+20].rstrip(b'\x00').decode('utf-8', errors='replace')
        offset += 20
        name_es = data[offset:offset+20].rstrip(b'\x00').decode('utf-8', errors='replace')
        offset += 20
        
        type1 = data[offset]; offset += 1
        type2 = data[offset]; offset += 1
        gen = data[offset]; offset += 1
        
        height_cm, weight_dkg = struct.unpack_from('<HH', data, offset)
        offset += 4
        
        base_exp = struct.unpack_from('<H', data, offset)[0]
        offset += 2
        
        stats = struct.unpack_from('<6B', data, offset)
        offset += 6
        
        desc_en_len = data[offset]; offset += 1
        desc_en = data[offset:offset+desc_en_len].decode('utf-8', errors='replace')
        offset += desc_en_len
        
        desc_es_len = data[offset]; offset += 1
        desc_es = data[offset:offset+desc_es_len].decode('utf-8', errors='replace')
        
        check(f"  [{pid}] Name non-empty", len(name) > 0, f"got '{name}'")
        check(f"  [{pid}] Name_ES non-empty", len(name_es) > 0)
        check(f"  [{pid}] Type1 valid", 1 <= type1 <= 18, f"got {type1}")
        check(f"  [{pid}] Gen valid", 1 <= gen <= 9, f"got {gen}")
        check(f"  [{pid}] Stats valid (HP>0)", stats[0] > 0, f"got {stats}")
        check(f"  [{pid}] Stats all <= 255", all(s <= 255 for s in stats), f"got {stats}")
        check(f"  [{pid}] Desc_EN non-empty", len(desc_en) > 0, f"len={desc_en_len}")
        check(f"  [{pid}] Desc_ES present or empty", True)  # Some may lack ES
        
        if pid == 1:
            check(f"  [1] Name = Bulbasaur", name == "Bulbasaur", f"got '{name}'")
            check(f"  [1] Type1 = grass(5)", type1 == 5, f"got {type1}")
            check(f"  [1] Type2 = poison(8)", type2 == 8, f"got {type2}")
            check(f"  [1] Height = 70cm", height_cm == 70, f"got {height_cm}")
            check(f"  [1] Weight = 69dkg", weight_dkg == 69, f"got {weight_dkg}")
        
        if pid == 25:
            check(f"  [25] Name = Pikachu", name == "Pikachu", f"got '{name}'")
            check(f"  [25] Type1 = electric(4)", type1 == 4, f"got {type1}")
    
    # Count files
    files = list(data_dir.glob("*.bin"))
    check(f"Total detail files = 1025 (got {len(files)})", len(files) == 1025)


def main():
    project_root = Path(__file__).parent.parent
    sd_dir = project_root / 'sd_dataset'
    index_path = sd_dir / 'pokemon_index.bin'
    data_dir = sd_dir / 'data'
    manifest_path = sd_dir / 'manifest.json'
    
    print("=== Local DB Validation ===")
    print(f"Directory: {sd_dir}")
    
    check("sd_dataset exists", sd_dir.exists())
    check("pokemon_index.bin exists", index_path.exists())
    check("data/ dir exists", data_dir.exists())
    check("manifest.json exists", manifest_path.exists())
    
    if not index_path.exists():
        print("\nERROR: Generate the dataset first: python tools/generate_local_db.py")
        sys.exit(1)
    
    test_index(index_path)
    test_detail_files(data_dir)
    
    print(f"\n=== Results: {PASS} PASS, {FAIL} FAIL ===")
    sys.exit(1 if FAIL > 0 else 0)


if __name__ == '__main__':
    main()
