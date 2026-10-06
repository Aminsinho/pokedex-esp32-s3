import re
data = open('tools/app_dump.bin','rb').read()
for m in re.finditer(rb'2\.8_ESP32S3_AP', data):
    off = m.start()
    chunk = data[max(0,off-300):off+400]
    # extract printable strings in chunk
    ss = re.findall(rb'[\x20-\x7e]{4,}', chunk)
    print('--- offset', hex(off))
    for s in ss:
        print(' ', s.decode()[:120])
