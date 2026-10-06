#!/usr/bin/env python3
"""send_to_sd.py — Push-based file transfer to ESP32 SD card."""
import os, sys, time, argparse, serial

BASE_PATH = "/pokedex/pokemon/"
CHUNK = 128

def read_line(ser, timeout=10):
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        if ser.in_waiting > 0:
            c = ser.read(1)
            buf += c
            if c == b'\n':
                return buf.decode().strip()
        else:
            time.sleep(0.002)
    return buf.decode().strip() if buf else "TIMEOUT"

def send_file(ser, path, data):
    ser.write(f"BEGIN {path} {len(data)}\r\n".encode())
    resp = read_line(ser, timeout=5)
    if resp != "READY":
        return False, resp

    # Send data in small chunks
    sent = 0
    while sent < len(data):
        chunk = data[sent:sent+CHUNK]
        ser.write(chunk)
        sent += len(chunk)
        time.sleep(0.005)
    ser.flush()

    # Wait for ESP32 to finish writing to SD
    time.sleep(0.3)
    
    # Read any pending response
    resp = read_line(ser, timeout=5)
    if resp == "OK":
        return True, "OK"
    
    # Verify via SIZE
    ser.write(f"SIZE {path}\r\n".encode())
    resp2 = read_line(ser, timeout=5)
    if resp2.startswith("SIZE_") and int(resp2[5:]) == len(data):
        return True, "OK(verified)"
    
    return False, f"{resp} {resp2}"

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="COM4")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--base", default=BASE_PATH)
    p.add_argument("--dataset", default=None)
    args = p.parse_args()

    dataset = args.dataset or os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sd_dataset"))
    if not os.path.isdir(dataset):
        print(f"ERROR: {dataset} not found"); sys.exit(1)

    print(f"Connecting to {args.port}...")
    ser = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(0.5)

    # Reset
    ser.dtr = True; time.sleep(0.1); ser.dtr = False
    ser.rts = True; time.sleep(0.2); ser.rts = False
    time.sleep(3)

    # Read banner
    ser.reset_input_buffer(); time.sleep(0.3)
    if ser.in_waiting > 0:
        print(f"ESP32: {ser.read(ser.in_waiting).decode(errors='replace').strip()}")

    # Mkdirs
    ser.write(b"MKDIR " + args.base.encode() + b"\r\n")
    print(f"  mkdir: {read_line(ser, 3)}")
    ser.write(b"MKDIR " + (args.base + "data").encode() + b"\r\n")
    print(f"  mkdir: {read_line(ser, 3)}")

    # Index
    idx = os.path.join(dataset, "pokemon_index.bin")
    with open(idx, "rb") as f: data = f.read()
    print(f"Sending index ({len(data)} B)...")
    ok, resp = send_file(ser, args.base + "pokemon_index.bin", data)
    print(f"  {'OK' if ok else 'FAIL'}: {resp}")
    if not ok: ser.close(); sys.exit(1)

    # Data files
    ddir = os.path.join(dataset, "data")
    files = sorted(os.listdir(ddir)) if os.path.isdir(ddir) else []
    print(f"Sending {len(files)} data files...")
    
    ok_count = 0; fail_count = 0
    t0 = time.time()
    for i, fn in enumerate(files):
        with open(os.path.join(ddir, fn), "rb") as f: data = f.read()
        ok, resp = send_file(ser, args.base + "data/" + fn, data)
        if ok: ok_count += 1
        else:
            fail_count += 1
            print(f"  FAIL {fn}: {resp}")
            if fail_count > 5: print("Too many errors, aborting"); break
        if (i+1) % 200 == 0:
            print(f"  {i+1}/{len(files)} ({time.time()-t0:.0f}s)")
    
    print(f"\nResult: {ok_count} OK, {fail_count} FAIL, {time.time()-t0:.1f}s")
    ser.write(b"END\r\n")
    read_line(ser, 3)
    ser.close()
    print("=== DONE ===")

if __name__ == "__main__":
    main()
