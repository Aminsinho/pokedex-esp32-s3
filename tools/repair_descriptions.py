#!/usr/bin/env python3
"""Repair legacy UTF-8 text that was decoded repeatedly as Windows-1252."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "backend/app/data/pokemon.json"

def repair(text):
    if not isinstance(text, str):
        return text
    for _ in range(4):
        try:
            decoded = text.encode("cp1252").decode("utf-8")
        except (UnicodeEncodeError, UnicodeDecodeError):
            break
        if decoded == text:
            break
        text = decoded
    return text

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    records = json.loads(DATA.read_text(encoding="utf-8"))
    changed = 0
    for pokemon in records:
        for key in ("name_es", "description_en", "description_es"):
            old = pokemon.get(key)
            new = repair(old)
            if old != new:
                pokemon[key] = new
                changed += 1
    bad = [(p["id"], key) for p in records for key in ("name_es", "description_en", "description_es")
           if isinstance(p.get(key), str) and any(mark in p[key] for mark in ("Ã", "Â", "�"))]
    if bad:
        raise SystemExit(f"unrepaired fields: {bad[:10]}")
    print(f"repaired fields: {changed}; records: {len(records)}")
    if not args.check:
        DATA.write_text(json.dumps(records, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()
