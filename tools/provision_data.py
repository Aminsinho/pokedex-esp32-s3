#!/usr/bin/env python3
"""Provision validated detail records or narration through bounded USB ACKs."""
import argparse
import time
from pathlib import Path
import serial

ROOT = Path(__file__).resolve().parents[1]
CHUNK = 128

def line(port, timeout=5):
    deadline, data = time.monotonic() + timeout, bytearray()
    while time.monotonic() < deadline:
        byte = port.read(1)
        if byte == b'\n': return data.decode(errors='replace').strip()
        if byte not in (b'', b'\r'): data += byte
    return data.decode(errors='replace').strip() or 'TIMEOUT'

def expect(port, wanted, timeout=8):
    deadline = time.monotonic() + timeout
    seen = []
    while time.monotonic() < deadline:
        value = line(port, min(2, max(.2, deadline - time.monotonic())))
        if value == wanted or value.startswith(wanted): return value
        if value != 'TIMEOUT': seen.append(value)
    raise RuntimeError(f'expected {wanted}; received {seen[-4:]}')

def fnv1a(data):
    value = 2166136261
    for byte in data: value = ((value ^ byte) * 16777619) & 0xffffffff
    return value

def send(port, command, data):
    # command is 'KIND ID' (e.g. 'MECH 25'); firmware expects 'KIND ID LENGTH'
    port.write(f'{command} {len(data)}\n'.encode())
    try: expect(port, 'DATA READY')
    except RuntimeError as error: raise RuntimeError(f'{command}: device not ready ({error})') from error
    for offset in range(0, len(data), CHUNK):
        port.write(data[offset:offset + CHUNK])
        try: expect(port, 'DATA CHUNK')
        except RuntimeError as error: raise RuntimeError(f'{command}: chunk {offset} failed ({error})') from error
    deadline = time.monotonic() + 20
    result = 'TIMEOUT'
    while time.monotonic() < deadline:
        candidate = line(port, 2)
        if candidate.startswith('DATA OK') or candidate.startswith('DATA ERROR'):
            result = candidate
            break
    expected = f'DATA OK {fnv1a(data):08x}'
    if result.lower() != expected.lower(): raise RuntimeError(f'{command}: {result}, expected {expected}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM4')
    parser.add_argument('--details', action='store_true')
    parser.add_argument('--mechanics', action='store_true')
    parser.add_argument('--stones', action='store_true')
    parser.add_argument('--start', type=int, default=1)
    parser.add_argument('--narration', type=int)
    parser.add_argument('--narration-first', type=int)
    parser.add_argument('--narration-last', type=int)
    args = parser.parse_args()
    narration_range = args.narration_first is not None or args.narration_last is not None
    if not (args.details or args.mechanics or args.stones or narration_range) and args.narration is None:
        parser.error('select --details, --mechanics, --stones, --narration ID, or a narration range')
    if narration_range and (args.narration_first is None or args.narration_last is None):
        parser.error('--narration-first and --narration-last must be used together')
    if narration_range and not (1 <= args.narration_first <= args.narration_last <= 1025):
        parser.error('narration range must be between 1 and 1025')
    files = []
    if args.details:
        files += [(f'@DATA {int(p.stem)}', p) for p in sorted((ROOT / 'sd_dataset/data').glob('*.bin')) if int(p.stem) >= args.start]
    if args.mechanics:
        files += [(f'@MECH {int(p.stem)}', p) for p in sorted((ROOT / 'sd_dataset/pokemon/mechanics/data').glob('*.bin')) if int(p.stem) >= args.start]
    if args.stones:
        files += [(f'@STONE {int(p.stem)}', p) for p in sorted((ROOT / 'sd_dataset/ui/stones').glob('*.r565'))]
    if args.narration is not None:
        p = ROOT / f'sd_dataset/audio/narration/{args.narration:04}.wav'
        files.append((f'@NARR {args.narration}', p))
    if narration_range:
        files += [
            (f'@NARR {pokemon_id}', ROOT / f'sd_dataset/audio/narration/{pokemon_id:04}.wav')
            for pokemon_id in range(args.narration_first, args.narration_last + 1)
        ]
    missing = [str(path) for _, path in files if not path.is_file()]
    if missing:
        raise FileNotFoundError(f'missing {len(missing)} input file(s); first: {missing[0]}')
    port = serial.Serial(port=None, baudrate=115200, timeout=.2, write_timeout=5)
    port.dtr = False; port.rts = False; port.port = args.port
    with port:
        time.sleep(1); port.reset_input_buffer()
        started = time.monotonic()
        for index, (command, path) in enumerate(files, 1):
            send(port, command, path.read_bytes())
            if index % 100 == 0 or index == len(files): print(f'{index}/{len(files)} verified')
        print(f'done in {time.monotonic() - started:.1f}s')

if __name__ == '__main__':
    main()
