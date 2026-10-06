#!/usr/bin/env python3
"""
convert_sprites.py — Convert PNG sprites to raw RGB565 format for ESP32 display.

Output format (per file):
  Bytes 0-3:   Width  (uint32_t LE)
  Bytes 4-7:   Height (uint32_t LE)
  Bytes 8+:   RGB565 pixel data (row-major, top-to-bottom, left-to-right)

Usage:
  python3 tools/convert_sprites.py [--input DIR] [--output DIR] [--small-only] [--large-only]
"""
import os
import struct
import sys
import argparse

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed. Run: pip install Pillow")
    sys.exit(1)


def png_to_rgb565(src_path: str, dst_path: str) -> bool:
    """Convert a single PNG to raw RGB565 with 8-byte header."""
    try:
        img = Image.open(src_path).convert("RGBA")
    except Exception as e:
        print(f"  FAIL: {src_path}: {e}")
        return False

    w, h = img.size
    # Allocate RGB565 buffer
    buf = bytearray(w * h * 2)

    pixels = img.load()
    idx = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if a < 16:
                buf[idx] = 0
                buf[idx + 1] = 0
            else:
                r5 = (r >> 3) & 0x1F
                g6 = (g >> 2) & 0x3F
                b5 = (b >> 3) & 0x1F
                v = (r5 << 11) | (g6 << 5) | b5
                if a < 255:
                    v = int(v * (a / 255.0))
                buf[idx] = v & 0xFF
                buf[idx + 1] = (v >> 8) & 0xFF
            idx += 2

    # Write header + data
    with open(dst_path, 'wb') as f:
        f.write(struct.pack('<II', w, h))
        f.write(buf)

    return True


def main():
    parser = argparse.ArgumentParser(description="Convert PNG sprites to RGB565 for ESP32")
    parser.add_argument("--input", default=None, help="Input dir (default: auto-detect)")
    parser.add_argument("--output", default=None, help="Output dir (default: alongside input with .r565 ext)")
    parser.add_argument("--small-only", action="store_true")
    parser.add_argument("--large-only", action="store_true")
    args = parser.parse_args()

    # Detect input directories
    base = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "backend", "storage", "assets")
    small_dir = os.path.join(base, "small")
    large_dir = os.path.join(base, "large")

    dirs = []
    if not args.large_only and os.path.isdir(small_dir):
        dirs.append(small_dir)
    if not args.small_only and os.path.isdir(large_dir):
        dirs.append(large_dir)

    if args.input:
        dirs = [args.input]

    if not dirs:
        print("ERROR: No sprite directories found")
        print(f"  Expected: {small_dir}")
        print(f"  Expected: {large_dir}")
        sys.exit(1)

    total = 0
    ok = 0
    fail = 0

    for d in dirs:
        files = sorted([f for f in os.listdir(d) if f.endswith('.png')])
        print(f"\n[{d}] {len(files)} PNG sprites")

        out_dir = args.output or d
        for fname in files:
            src = os.path.join(d, fname)
            dst = os.path.join(out_dir, fname.replace('.png', '.r565'))
            if png_to_rgb565(src, dst):
                ok += 1
            else:
                fail += 1
            total += 1

    print(f"\n=== DONE ===")
    print(f"Total: {total}, OK: {ok}, FAIL: {fail}")

    if ok > 0:
        # Show sample file size
        sample = os.path.join(out_dir, files[0].replace('.png', '.r565'))
        if os.path.exists(sample):
            sz = os.path.getsize(sample)
            print(f"Sample: {os.path.basename(sample)} = {sz} bytes ({sz/1024:.1f} KB)")


if __name__ == "__main__":
    main()
