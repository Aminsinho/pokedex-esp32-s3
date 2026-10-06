"""Capture actual LVGL flush pixels via USB; no mock rendering or webcam access."""
import argparse
import time
from pathlib import Path
import serial
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
parser.add_argument("--port", default="COM4")
parser.add_argument("--view", choices=["HOME", "LIST", "SCAN"])
parser.add_argument("--detail", type=int)
args = parser.parse_args()
port = serial.Serial()
port.port, port.baudrate, port.timeout = args.port, 115200, 1
port.dtr = port.rts = False
port.open()
try:
    if args.detail:
        port.write(f"@DETAIL {args.detail}\n".encode())
    elif args.view:
        port.write(f"@{args.view}\n".encode())
    time.sleep(1)
    port.reset_input_buffer()
    port.write(b"@SNAP\n")
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        line = port.readline()
        if line.startswith(b"UIFRAME 320 240 153600"):
            break
    else:
        raise RuntimeError("snapshot header timeout")
    data = bytearray()
    while len(data) < 153600 and time.monotonic() < deadline:
        data.extend(port.read(153600 - len(data)))
    if len(data) != 153600:
        raise RuntimeError(f"incomplete frame: {len(data)} bytes")
finally:
    port.close()
rgb = bytearray(320 * 240 * 3)
for i in range(320 * 240):
    pixel = data[2*i] | data[2*i+1] << 8
    rgb[3*i:3*i+3] = bytes(((pixel >> 11) * 255 // 31,
                           ((pixel >> 5) & 63) * 255 // 63, (pixel & 31) * 255 // 31))
args.output.parent.mkdir(parents=True, exist_ok=True)
Image.frombytes("RGB", (320, 240), bytes(rgb)).save(args.output)
print(args.output.resolve())
