#!/usr/bin/env python3
"""QA visual de las 4 pestanas de la ficha por generaciones (FICHA/EVO/ESTADIS/ATAQUES).

Provisiona @MECH, abre @DETAIL, y captura cada pestana con @SNAP -> PNG.

Uso:
    python tools/qa_tabs.py --id 1
    python tools/qa_tabs.py --id 133          # Eevee (9 nodos de evolucion)
    python tools/qa_tabs.py --id 25 --tabs 0,3
"""
from __future__ import annotations

import argparse
import time
from pathlib import Path

import serial
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
W, H = 320, 240
NAMES = {0: "ficha", 1: "evo", 2: "estadis", 3: "ataques"}
CHUNK = 128


def expect(port, wanted, timeout=10):
    deadline = time.monotonic() + timeout
    seen = []
    while time.monotonic() < deadline:
        line = b""
        local = time.monotonic()
        while time.monotonic() < min(local + 2, deadline):
            b = port.read(1)
            if b == b"\n":
                break
            if b and b not in (b"\r",):
                line += b
        s = line.decode(errors="replace").strip()
        if s and (s == wanted or s.startswith(wanted)):
            return s
        if s:
            seen.append(s)
    raise RuntimeError(f"esperado {wanted!r}; visto {seen[-6:]}")


def send_file(port, kind, pid, path: Path):
    data = path.read_bytes()
    port.write(f"@{kind} {pid} {len(data)}\n".encode())
    port.flush()
    expect(port, "DATA READY", 8)
    off = 0
    while off < len(data):
        port.write(data[off:off + CHUNK])
        port.flush()
        off += CHUNK
        expect(port, "DATA CHUNK", 8)
    expect(port, "DATA OK", 25)


def ui(port, s, settle=0.4):
    port.write((s + "\n").encode())
    port.flush()
    time.sleep(settle)


def snap(port):
    port.reset_input_buffer()
    port.write(b"@SNAP\n")
    port.flush()
    hdr = b""
    dl = time.monotonic() + 12
    while time.monotonic() < dl:
        c = port.read(1)
        if c:
            hdr += c
            if c == b"\n" and b"UIFRAME" in hdr:
                break
        else:
            if b"UIFRAME" in hdr:
                break
            time.sleep(0.02)
    parts = hdr.decode(errors="replace").split()
    if not (parts and parts[0].startswith("UIFRAME")):
        raise RuntimeError(f"sin cabecera UIFRAME: {hdr!r}")
    w, h, n = int(parts[1]), int(parts[2]), int(parts[3])
    buf = b""
    dl = time.monotonic() + 40
    while len(buf) < n and time.monotonic() < dl:
        chunk = port.read(min(4096, n - len(buf)))
        if chunk:
            buf += chunk
        else:
            time.sleep(0.01)
    tail = b""
    dl = time.monotonic() + 5
    while time.monotonic() < dl:
        c = port.read(1)
        if c:
            tail += c
            if c == b"\n" and b"UIEND" in tail:
                break
        else:
            if b"UIEND" in tail:
                break
            time.sleep(0.02)
    return buf[:n], w, h


def to_png(raw: bytes, out: Path):
    if len(raw) < W * H * 2:
        raw = raw.ljust(W * H * 2, b"\x00")
    px = bytearray()
    for i in range(0, W * H * 2, 2):
        v = raw[i] | (raw[i + 1] << 8)
        r = (v >> 11) & 0x1F
        g = (v >> 5) & 0x3F
        b = v & 0x1F
        px += bytes((r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2))
    Image.frombytes("RGB", (W, H), bytes(px)).save(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--id", type=int, default=1)
    ap.add_argument("--port", default="COM4")
    ap.add_argument("--tabs", default="0,1,2,3")
    ap.add_argument("--skip-provision", action="store_true")
    args = ap.parse_args()

    tabs = [int(t) for t in args.tabs.split(",") if t.strip() != ""]
    outdir = ROOT / "build-sinnoh"
    outdir.mkdir(parents=True, exist_ok=True)

    port = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=5)
    port.dtr = False
    port.rts = False
    port.port = args.port
    with port:
        time.sleep(1.5)
        port.reset_input_buffer()

        if not args.skip_provision:
            mech = ROOT / "sd_dataset" / "pokemon" / "mechanics" / "data" / f"{args.id:04d}.bin"
            if not mech.exists():
                raise SystemExit(f"no existe {mech}")
            print(f"provisionando @MECH {args.id} ({mech.stat().st_size} b) ...")
            send_file(port, "MECH", args.id, mech)
            print("provision OK")

        ui(port, f"@DETAIL {args.id}", settle=0.8)
        for t in tabs:
            if t != 0:
                ui(port, f"@TAB {t}", settle=0.5)
            data, w, h = snap(port)
            path = outdir / f"qa_{args.id}_{NAMES.get(t, t)}.png"
            to_png(data, path)
            print(f"tab {t} ({NAMES.get(t, t)}) -> {path}  ({len(data)} raw)")

        port.reset_input_buffer()
    print("OK")


if __name__ == "__main__":
    main()
