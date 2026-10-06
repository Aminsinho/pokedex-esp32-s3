#!/usr/bin/env python3
"""
PokéAPI Sync Tool — downloads and normalizes all Pokémon data + sprites.

Usage:
    python tools/sync_pokemon_data.py --kanto          # Sync 1-151
    python tools/sync_pokemon_data.py --all            # Sync all
    python tools/sync_pokemon_data.py --range 1 151    # Custom range
    python tools/sync_pokemon_data.py --sprites-only   # Only reprocess sprites
    python tools/sync_pokemon_data.py --manifest-only  # Only regenerate manifest

Resumable: skips Pokémon already correctly cached.
"""

import argparse
import hashlib
import json
import re
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import quote

import requests
from PIL import Image, ImageOps

# ─── CONFIG ───────────────────────────────────────────────────────────────────

BASE_DIR = Path(__file__).resolve().parent.parent
DATA_DIR = BASE_DIR / "backend" / "app" / "data"
ASSETS_DIR = BASE_DIR / "backend" / "storage" / "assets"
SPRITES_SMALL_DIR = ASSETS_DIR / "small"
SPRITES_LARGE_DIR = ASSETS_DIR / "large"
RAW_DIR = ASSETS_DIR / "raw"
MANIFEST_FILE = DATA_DIR / "manifest.json"
VERSION_FILE = DATA_DIR / "version.json"

POKEAPI_BASE = "https://pokeapi.co/api/v2"
TIMEOUT = 15
MAX_RETRIES = 3
RATE_LIMIT_DELAY = 0.25  # seconds between API calls
MAX_CONCURRENT_SPRITES = 4

SMALL_SIZE = 48  # px
LARGE_SIZE = 96  # px

# ─── SESSION ──────────────────────────────────────────────────────────────────

session = requests.Session()
session.headers.update({"User-Agent": "PokedexBackend/0.2 (sync tool)"})


def get_json(url: str, params: dict | None = None) -> dict | None:
    """GET JSON with retry + rate limiting."""
    for attempt in range(MAX_RETRIES):
        try:
            resp = session.get(url, params=params, timeout=TIMEOUT)
            if resp.status_code == 429:
                wait = int(resp.headers.get("Retry-After", "2"))
                time.sleep(wait)
                continue
            resp.raise_for_status()
            time.sleep(RATE_LIMIT_DELAY)
            return resp.json()
        except (requests.RequestException, requests.HTTPError) as e:
            if attempt < MAX_RETRIES - 1:
                time.sleep(1 + attempt)
            else:
                print(f"  [ERR] GET {url}: {e}", file=sys.stderr)
                return None
    return None


def download_image(url: str, save_path: Path) -> bool:
    """Download image to file."""
    if save_path.exists():
        return True
    save_path.parent.mkdir(parents=True, exist_ok=True)
    for attempt in range(MAX_RETRIES):
        try:
            resp = session.get(url, timeout=TIMEOUT)
            resp.raise_for_status()
            save_path.write_bytes(resp.content)
            time.sleep(0.1)
            return True
        except requests.RequestException:
            if attempt < MAX_RETRIES - 1:
                time.sleep(1 + attempt)
    return False


# ─── NORMALIZATION ────────────────────────────────────────────────────────────

def clean_description(text: str) -> str:
    """Clean PokéAPI flavor text (remove \n, \f, extra spaces)."""
    if not text:
        return ""
    text = text.replace("\n", " ").replace("\f", " ")
    text = re.sub(r"\s{2,}", " ", text).strip()
    return text


def get_flavor_text(species_data: dict, lang: str) -> str:
    """Get cleaned flavor text for a language."""
    for entry in species_data.get("flavor_text_entries", []):
        if entry.get("language", {}).get("name") == lang:
            return clean_description(entry.get("flavor_text", ""))
    return ""


def get_name_localized(names: list[dict], lang: str) -> str | None:
    """Get localized name."""
    for n in names:
        if n.get("language", {}).get("name") == lang:
            return n.get("name")
    return None


def normalize_pokemon(poke_data: dict, species_data: dict) -> dict:
    """Normalize PokéAPI pokemon + species into our schema."""
    pokemon_id = poke_data["id"]
    
    # Types
    types = [t["type"]["name"] for t in poke_data.get("types", [])]
    
    # Stats (6 mandatory)
    stats_raw = {}
    for s in poke_data.get("stats", []):
        stat_name = s["stat"]["name"]
        stats_raw[stat_name] = s["base_stat"]
    
    stats = {
        "hp": stats_raw.get("hp", 0),
        "attack": stats_raw.get("attack", 0),
        "defense": stats_raw.get("defense", 0),
        "special_attack": stats_raw.get("special-attack", stats_raw.get("special_attack", 0)),
        "special_defense": stats_raw.get("special-defense", stats_raw.get("special_defense", 0)),
        "speed": stats_raw.get("speed", 0),
    }
    
    # Abilities
    abilities = []
    for a in poke_data.get("abilities", []):
        abilities.append({
            "name": a["ability"]["name"],
            "slot": a.get("slot", 1),
        })
    
    # Generation
    generation = ""
    gen = species_data.get("generation", {})
    if isinstance(gen, dict):
        generation = gen.get("name", "")
    
    # Descriptions
    description_en = get_flavor_text(species_data, "en")
    description_es = get_flavor_text(species_data, "es") or None
    
    # Names (capitalize first letter for display)
    name = poke_data["name"].capitalize()
    name_es = get_name_localized(species_data.get("names", []), "es")
    
    return {
        "id": pokemon_id,
        "name": name,
        "name_es": name_es,
        "types": types,
        "height": poke_data.get("height", 0) / 10.0,
        "weight": poke_data.get("weight", 0) / 10.0,
        "base_experience": poke_data.get("base_experience", 0),
        "generation": generation,
        "abilities": abilities,
        "stats": stats,
        "description_en": description_en,
        "description_es": description_es,
    }


# ─── SPRITES ──────────────────────────────────────────────────────────────────

def get_sprite_url(poke_data: dict) -> str | None:
    """Get the best sprite URL from PokéAPI data."""
    sprites = poke_data.get("sprites", {})
    # Prefer official artwork (no animation, high quality)
    url = sprites.get("other", {}).get("official-artwork", {}).get("front_default")
    if url:
        return url
    # Fallback to front_default
    url = sprites.get("front_default")
    if url:
        return url
    # Fallback to animated
    url = sprites.get("versions", {}).get("generation-v", {}).get("black-white", {}).get("animated", {}).get("front_default")
    return url


def process_sprite(raw_path: Path, pokemon_id: int) -> tuple[bool, bool]:
    """Process a raw sprite into small and large versions. Returns (small_ok, large_ok)."""
    if not raw_path.exists():
        return False, False
    
    try:
        img = Image.open(raw_path)
        img = ImageOps.exif_transpose(img)
        
        # Convert to RGBA if needed
        if img.mode not in ("RGBA", "RGB"):
            img = img.convert("RGBA")
        
        # Small sprite
        small_path = SPRITES_SMALL_DIR / f"{pokemon_id:04d}.png"
        if not small_path.exists():
            small = img.resize((SMALL_SIZE, SMALL_SIZE), Image.LANCZOS)
            small.save(small_path, "PNG", optimize=True)
        
        # Large sprite
        large_path = SPRITES_LARGE_DIR / f"{pokemon_id:04d}.png"
        if not large_path.exists():
            large = img.resize((LARGE_SIZE, LARGE_SIZE), Image.LANCZOS)
            large.save(large_path, "PNG", optimize=True)
        
        return small_path.exists(), large_path.exists()
    except Exception as e:
        print(f"  [ERR] Sprite processing #{pokemon_id:04d}: {e}", file=sys.stderr)
        return False, False


# ─── SYNC LOGIC ───────────────────────────────────────────────────────────────

def get_total_count() -> int:
    """Get total Pokémon count from PokéAPI."""
    data = get_json(f"{POKEAPI_BASE}/pokemon", params={"limit": 1, "offset": 0})
    if data:
        return data.get("count", 0)
    return -1


def pokemon_is_cached(pokemon_id: int) -> bool:
    """Check if a Pokémon is already in our data file."""
    data_file = DATA_DIR / "pokemon.json"
    if not data_file.exists():
        return False
    try:
        with open(data_file, encoding="utf-8") as f:
            data = json.load(f)
        return any(p["id"] == pokemon_id for p in data)
    except (json.JSONDecodeError, FileNotFoundError):
        return False


def sprite_is_complete(pokemon_id: int) -> bool:
    """Check if both sprites exist."""
    small = SPRITES_SMALL_DIR / f"{pokemon_id:04d}.png"
    large = SPRITES_LARGE_DIR / f"{pokemon_id:04d}.png"
    return small.exists() and large.exists()


def sync_pokemon_data(ids: list[int]) -> tuple[int, int]:
    """Sync Pokémon data from PokéAPI. Returns (success_count, error_count)."""
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    data_file = DATA_DIR / "pokemon.json"
    
    # Load existing data
    existing = {}
    if data_file.exists():
        try:
            with open(data_file, encoding="utf-8") as f:
                for p in json.load(f):
                    existing[p["id"]] = p
        except (json.JSONDecodeError, FileNotFoundError):
            pass
    
    success = 0
    errors = 0
    
    for i, pid in enumerate(ids):
        if (i + 1) % 50 == 0 or i == 0:
            print(f"  [{i+1}/{len(ids)}] Syncing data...")
        
        if pid in existing and existing[pid].get("stats", {}).get("special_attack", 0) > 0:
            success += 1
            continue  # Already cached with 6 stats
        
        # Fetch pokemon
        poke_data = get_json(f"{POKEAPI_BASE}/pokemon/{pid}")
        if not poke_data:
            errors += 1
            continue
        
        # Fetch species
        species_data = get_json(f"{POKEAPI_BASE}/pokemon-species/{pid}")
        if not species_data:
            species_data = {}
        
        # Normalize
        normalized = normalize_pokemon(poke_data, species_data)
        existing[pid] = normalized
        success += 1
    
    # Write sorted by ID
    sorted_data = [existing[k] for k in sorted(existing.keys())]
    with open(data_file, "w", encoding="utf-8") as f:
        json.dump(sorted_data, f, indent=2, ensure_ascii=False)
    
    print(f"  Data: {success} OK, {errors} errors, total in DB: {len(existing)}")
    return success, errors


def sync_sprites(ids: list[int]) -> tuple[int, int]:
    """Download and process sprites. Returns (success, errors)."""
    SPRITES_SMALL_DIR.mkdir(parents=True, exist_ok=True)
    SPRITES_LARGE_DIR.mkdir(parents=True, exist_ok=True)
    RAW_DIR.mkdir(parents=True, exist_ok=True)
    
    success = 0
    errors = 0
    
    for i, pid in enumerate(ids):
        if (i + 1) % 25 == 0 or i == 0:
            print(f"  [{i+1}/{len(ids)}] Syncing sprites...")
        
        if sprite_is_complete(pid):
            success += 1
            continue
        
        raw_path = RAW_DIR / f"{pid:04d}.png"
        
        if not raw_path.exists():
            # Need to fetch from API to get sprite URL
            poke_data = get_json(f"{POKEAPI_BASE}/pokemon/{pid}")
            if not poke_data:
                errors += 1
                continue
            sprite_url = get_sprite_url(poke_data)
            if not sprite_url:
                errors += 1
                continue
            if not download_image(sprite_url, raw_path):
                errors += 1
                continue
        
        small_ok, large_ok = process_sprite(raw_path, pid)
        if small_ok and large_ok:
            success += 1
        else:
            errors += 1
    
    print(f"  Sprites: {success} OK, {errors} errors")
    return success, errors


def generate_manifest() -> dict:
    """Generate the asset manifest."""
    data_file = DATA_DIR / "pokemon.json"
    manifest = {
        "version": "1.0",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "pokemon_count": 0,
        "asset_format": "png",
        "pokemon": [],
    }
    
    if data_file.exists():
        with open(data_file, encoding="utf-8") as f:
            data = json.load(f)
        manifest["pokemon_count"] = len(data)
        
        for p in data:
            pid = p["id"]
            small = SPRITES_SMALL_DIR / f"{pid:04d}.png"
            large = SPRITES_LARGE_DIR / f"{pid:04d}.png"
            entry = {
                "id": pid,
                "name": p["name"],
                "small_sprite": f"small/{pid:04d}.png" if small.exists() else None,
                "large_sprite": f"large/{pid:04d}.png" if large.exists() else None,
            }
            manifest["pokemon"].append(entry)
    
    MANIFEST_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(MANIFEST_FILE, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    
    # Also write version file
    version = {
        "dataset_version": manifest["generated_at"],
        "pokemon_count": manifest["pokemon_count"],
    }
    with open(VERSION_FILE, "w", encoding="utf-8") as f:
        json.dump(version, f, indent=2)
    
    print(f"  Manifest: {manifest['pokemon_count']} Pokémon, {len(manifest['pokemon'])} entries")
    return manifest


# ─── MAIN ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="PokéAPI Sync Tool")
    parser.add_argument("--kanto", action="store_true", help="Sync Kanto (1-151)")
    parser.add_argument("--all", action="store_true", help="Sync all Pokémon")
    parser.add_argument("--range", nargs=2, type=int, metavar=("START", "END"), help="Custom range")
    parser.add_argument("--sprites-only", action="store_true", help="Only process sprites")
    parser.add_argument("--manifest-only", action="store_true", help="Only regenerate manifest")
    parser.add_argument("--no-sprites", action="store_true", help="Skip sprite sync")
    args = parser.parse_args()
    
    print("=" * 60)
    print("POKÉAPI SYNC TOOL")
    print("=" * 60)
    
    # Determine ID range
    if args.manifest_only:
        generate_manifest()
        print("DONE (manifest only)")
        return
    
    if args.sprites_only:
        # Load existing IDs
        data_file = DATA_DIR / "pokemon.json"
        if data_file.exists():
            with open(data_file, encoding="utf-8") as f:
                ids = [p["id"] for p in json.load(f)]
        else:
            ids = list(range(1, 152))
    elif args.range:
        ids = list(range(args.range[0], args.range[1] + 1))
    elif args.kanto:
        ids = list(range(1, 152))
    elif args.all:
        # Try to load valid IDs from cache file first
        valid_ids_file = BASE_DIR / "valid_pokemon_ids.json"
        if valid_ids_file.exists():
            with open(valid_ids_file, encoding="utf-8") as f:
                ids = json.load(f)
            print(f"Loaded {len(ids)} valid IDs from cache")
        else:
            total = get_total_count()
            if total < 0:
                print("[ERR] Cannot get total count from PokéAPI")
                sys.exit(1)
            print(f"Total Pokémon available: {total}")
            ids = list(range(1, total + 1))
    else:
        # Default: Kanto
        ids = list(range(1, 152))
    
    print(f"Target: {len(ids)} Pokémon (IDs {ids[0]}–{ids[-1]})")
    
    # Sync data
    if not args.sprites_only:
        print("\n[1/3] Syncing Pokémon data...")
        sync_pokemon_data(ids)
    
    # Sync sprites
    if not args.no_sprites and not args.manifest_only:
        print("\n[2/3] Syncing sprites...")
        sync_sprites(ids)
    
    # Generate manifest
    print("\n[3/3] Generating manifest...")
    generate_manifest()
    
    print("\n" + "=" * 60)
    print("SYNC COMPLETE")
    print("=" * 60)


if __name__ == "__main__":
    main()
