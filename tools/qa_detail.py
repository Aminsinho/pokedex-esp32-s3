#!/usr/bin/env python3
"""QA de la ficha por generaciones: provisiona una especie, abre su ficha y captura el framebuffer.

Uso:  python tools/qa_detail.py --id 1 [--tab evolutions|stats|moves|ficha]
      (provisiona la PKME del id si falta en el dataset)
Salida: build-sinnoh/qa_<id>_<tab>.png  (RGB565 -> PNG)
"""
import argparse, struct, sys, time
from pathlib import Path
import serial

ROOT = Path(__file__).resolve().parents[1]
W, H = 320, 240

def expect(port, wanted, timeout=10):
    deadline = time.monotonic() + timeout
    seen = []
    while time.monotonic() < deadline:
        line = b''
        local = time.monotonic()
        while time.monotonic() < min(local + 2, deadline):
            b = port.read(1)
            if b == b'\n': break
            if b and b not in (b'\r',): line += b
        s = line.decode(errors='replace').strip()
        if s and (s == wanted or s.startswith(wanted)): return s
        if s: seen.append(s)
    raise RuntimeError(f'esperado {wanted!r}; visto {seen[-4:]}')

def send_file(port, kind, pid, path: Path):
    data = path.read_bytes()
    port.write(f'@{kind} {pid} {len(data)}\n'.encode())
    expect(port, 'DATA READY', 5)
    CHUNK = 128
    off = 0
    while off < len(data):
        port.write(data[off:off+CHUNK]); off += CHUNK
        expect(port, 'DATA CHUNK', 5)
    expect(port, 'DATA OK', 20)

def cmd(port, s, timeout=10):
    port.write((s + '\n').encode())
    expect(port, 'OK', timeout) if False else None
    # no ACK esperado para la mayoría; solo leer hasta quieto
    end = time.monotonic() + timeout
    out = []
    while time.monotonic() < end:
        if port.in_waiting:
            out.append(port.readline().decode(errors='replace').rstrip())
            if len(out) > 8: break
        else:
            if out: break
            time.sleep(0.05)
    return out

def snap(port):
    # protocolo: "\nUIFRAME 320 240 153600\r\n" + <N bytes> + "\nUIEND\r\n"
    port.reset_input_buffer()
    port.write(b'@SNAP\n')
    # 1) leer cabecera UIFRAME
    hdr = b''
    dl = time.monotonic() + 10
    while time.monotonic() < dl:
        if port.in_waiting:
            c = port.read(1)
            hdr += c
            if c == b'\n' and b'UIFRAME' in hdr:
                break
        else:
            if b'UIFRAME' in hdr: break
            time.sleep(0.02)
    parts = hdr.decode(errors='replace').split()
    n = int(parts[-1]) if parts and parts[0].startswith('UIFRAME') else W*H*2
    # 2) leer exactamente n bytes de framebuffer
    buf = b''
    dl = time.monotonic() + 40
    while len(buf) < n and time.monotonic() < dl:
        chunk = port.read(min(4096, n - len(buf)))
        if chunk: buf += chunk
        else: time.sleep(0.01)
    # 3) descartar hasta UIEND
    tail = b''
    dl = time.monotonic() + 5
    while time.monotonic() < dl:
        if port.in_waiting:
            c = port.read(1); tail += c
            if c == b'\n' and b'UIEND' in tail: break
        else:
            if b'UIEND' in tail: break
            time.sleep(0.02)
    return buf

def to_png(raw: bytes, out: Path):
    if len(raw) < W*H*2:
        raw = raw.ljust(W*H*2, b'\x00')
    px = bytearray()
    for i in range(0, W*H*2, 2):
        v = raw[i] | (raw[i+1] << 8)
        r = (v >> 11) & 0x1F
        g = (v >> 5) & 0x3F
        b = v & 0x1F
        px += bytes((r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2))
    try:
        from PIL import Image
        img = Image.frombytes('RGB', (W, H), bytes(px))
        img.save(out)
        print(f'PNG -> {out}  ({len(raw)} bytes raw)')
    except ImportError:
        out.write_bytes(raw)
        print(f'RAW -> {out} (PIL no disponible)')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--id', type=int, default=1)
    ap.add_argument('--tab', default='ficha', choices=['ficha','evolutions','stats','moves'])
    ap.add_argument('--port', default='COM4')
    ap.add_argument('--skip-provision', action='store_true')
    args = ap.parse_args()
    pid = args.id
    p = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=5)
    p.dtr=False; p.rts=False; p.port=args.port
    with p:
        time.sleep(1); p.reset_input_buffer()
        if not args.skip_provision:
            f = ROOT / 'sd_dataset/pokemon/mechanics/data' / f'{pid:04d}.bin'
            if f.exists():
                print(f'provisionando @{pid} ...')
                send_file(p, 'MECH', pid, f)
                print('OK')
        # abrir ficha
        print(cmd(p, f'@DETAIL {pid}', 8))
        time.sleep(1.0)
        # si no es la ficha, cambiar de pestaña vía comando de UI (si existe)
        if args.tab != 'ficha':
            print(cmd(p, f'@TAB {args.tab}', 8))
            time.sleep(0.6)
        raw = snap(p)
        out = ROOT / 'build-sinnoh' / f'qa_{pid}_{args.tab}.png'
        to_png(raw, out)

if __name__ == '__main__':
    main()
