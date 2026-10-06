import sys
data = open(r'C:\Users\aminj\AppData\Local\Arduino15\packages\esp32\tools\esp32s3-libs\3.3.11\lib\libnet80211.a','rb').read()
off = 8
count = 0
while off + 68 <= len(data) and count < 5:
    name_b = data[off:off+16].decode('ascii','replace').strip()
    size_s = data[off+48:off+58].decode('ascii','replace').strip()
    magic = data[off+58:off+60]
    print(f'off={off} name={name_b!r} size={size_s!r} magic={magic!r}')
    if magic != b'`\n':
        break
    size = int(size_s)
    off += 60 + size
    off = (off + 1) & ~1
    count += 1
print('done, parsed', count, 'members')
