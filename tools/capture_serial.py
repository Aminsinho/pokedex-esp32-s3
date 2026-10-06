import serial, time, sys

PORT = 'COM4'
BAUD = 115200
DURATION = int(sys.argv[1]) if len(sys.argv) > 1 else 10

s = serial.serial_for_url(PORT, BAUD, timeout=0.5)
time.sleep(0.5)
s.reset_input_buffer()

# Toggle DTR/RTS to reset the ESP32 (auto-reset circuit)
s.dtr = False
s.rts = False
time.sleep(0.1)
s.dtr = True
time.sleep(0.1)
s.dtr = False
s.rts = True
time.sleep(0.05)
s.rts = False

end = time.time() + DURATION
buf = b''
while time.time() < end:
    d = s.read_all()
    if d:
        buf += d
s.close()
print(buf.decode(errors='replace'))
print(f"--- END ({len(buf)} bytes) ---")
