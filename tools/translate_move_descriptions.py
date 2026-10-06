#!/usr/bin/env python3
"""Traduce con Ollama los movimientos sin descripción oficial ES y guarda un diccionario revisable."""
import json
import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_mechanics as mech

OUT = Path(__file__).resolve().parent / "move_descriptions_es.json"


def ollama(batch):
    prompt = (
        "Traduce al español de España estas descripciones de movimientos Pokémon. "
        "Conserva el significado exacto, nombres propios y porcentajes. Devuelve SOLO un objeto JSON "
        "con los mismos IDs como claves y la traducción como valor.\n" +
        json.dumps(batch, ensure_ascii=False)
    )
    body = json.dumps({"model": "qwen3:30b", "prompt": prompt, "stream": False, "think": False,
                       "format": "json", "options": {"temperature": 0}}).encode()
    req = urllib.request.Request("http://127.0.0.1:11434/api/generate", data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=300) as response:
        return json.loads(json.loads(response.read())["response"])


def main():
    missing = {}
    for move_id in range(1, 1001):
        try:
            details = mech.move_details(move_id, True)
        except Exception:
            continue
        description, fallback = details[6], details[8]
        if fallback:
            missing[str(move_id)] = description
    translated = json.loads(OUT.read_text(encoding="utf-8")) if OUT.exists() else {}
    pending = [(key, value) for key, value in missing.items() if key not in translated]
    for start in range(0, len(pending), 12):
        chunk = dict(pending[start:start + 12])
        result = ollama(chunk)
        for key, value in result.items():
            if key in chunk and isinstance(value, str) and value.strip():
                translated[key] = value.strip()
        OUT.write_text(json.dumps(translated, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"{min(start + 12, len(pending))}/{len(pending)}")
    print(f"guardadas {len(translated)} traducciones en {OUT}")


if __name__ == "__main__":
    main()
