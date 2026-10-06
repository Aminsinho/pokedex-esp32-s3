"""Provision existing catalogue PNGs to SD in the display's RGB565 format.

No downloads, no dataset changes. Technical alpha compositing avoids the old
converter's multiplication of packed RGB565 (which produced rainbow edges).
"""
import argparse
import struct
import time
from pathlib import Path

import serial
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def encode(path, side, background):
    with Image.open(path) as source:
        rgba = source.convert("RGBA")
        if rgba.size != (side, side):
            raise ValueError(f"unexpected dimensions: {path}")
        image = Image.new("RGBA", rgba.size, background + (255,))
        image.alpha_composite(rgba)
        payload = bytearray(struct.pack("<II", side, side))
        for r, g, b, _ in image.getdata():
            payload.extend(struct.pack("<H", (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)))
        return bytes(payload)


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def response(port, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", "replace").strip()
        if line.startswith("SPRITE "):
            return line
    raise TimeoutError("sprite response timeout")


def send_blocks(port, data):
    """Never queue more than one acknowledged 128-byte CDC packet."""
    for offset in range(0, len(data), 128):
        port.write(data[offset:offset + 128])
        if response(port) != "SPRITE CHUNK":
            raise RuntimeError("sprite packet not acknowledged")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--first", type=int, default=1)
    parser.add_argument("--last", type=int, default=1025)
    args = parser.parse_args()
    if not 1 <= args.first <= args.last <= 1025:
        parser.error("expected ids 1..1025")
    port = serial.Serial()
    port.port, port.baudrate, port.timeout = args.port, 115200, .5
    port.dtr = port.rts = False
    port.open()
    written = 0
    try:
        port.reset_input_buffer()
        for id in range(args.first, args.last + 1):
            for kind, directory, side, background in [
                ("S", "small", 48, (227, 235, 240)),
                ("L", "large", 96, (134, 185, 188)),
            ]:
                path = ROOT / "backend/storage/assets" / directory / f"{id:04}.png"
                data = encode(path, side, background)
                port.write(f"@SPRITE {id} {kind} {len(data)}\n".encode())
                ready = response(port)
                if ready != "SPRITE READY": raise RuntimeError(ready)
                # Acknowledge bounded packets: no overflowing the 256-byte RX
                # queue, and no arbitrary sleeps throttling the whole import.
                send_blocks(port, data)
                result = response(port)
                if result != f"SPRITE OK {checksum(data):08x}":
                    raise RuntimeError(f"#{id} {kind}: {result}")
                written += 1
            if id % 50 == 0 or id == args.last:
                print(f"ID {id}: {written} sprites written and readback verified", flush=True)
    finally:
        port.close()


if __name__ == "__main__":
    main()
