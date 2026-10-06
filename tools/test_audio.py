"""Replay a short device beep over USB and record diagnostics. No SD writes."""
import argparse
import time
from pathlib import Path
import serial

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM4')
    args = parser.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    chunks = []
    with port:
        time.sleep(1)
        port.write(b'@BEEP\n')
        until = time.monotonic() + 12
        while time.monotonic() < until:
            data = port.read(4096)
            if data:
                chunks.append(data)
    output = b''.join(chunks).decode('utf-8', errors='replace')
    Path('audio-serial.log').write_text(output, encoding='utf-8')
    print(output)
    if '[AUDIO] test ready=1' not in output:
        raise SystemExit('Audio not ready or firmware command unavailable')

if __name__ == '__main__':
    main()
