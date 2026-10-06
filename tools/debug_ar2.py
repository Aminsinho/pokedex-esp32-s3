data = open(r'C:\Users\aminj\AppData\Local\Arduino15\packages\esp32\tools\esp32s3-libs\3.3.11\lib\libnet80211.a','rb').read()
off = 8
i = 0
while off + 68 <= len(data):
    name_b = data[off:off+16].decode('ascii','replace').strip()
    if len(name_b) > 2 and name_b.endswith('/') and not name_b.endswith('//'):
        name_b = name_b[:-1]
    size_s = data[off+48:off+58].decode('ascii','replace')
    magic = data[off+58:off+60]
    if magic[:1] != b'`':
        print(f'STOP at off={off}: name={name_b!r} sizefield={size_s!r} magic={magic!r}')
        print('context:', repr(data[off:off+70]))
        break
    try:
        size = int(size_s.strip())
    except ValueError:
        print(f'STOP at off={off}: bad size {size_s!r} name={name_b!r}')
        break
    nxt = off + 60 + size
    nxt = (nxt + 1) & ~1
    i += 1
    if i % 10 == 0:
        print(f'  ...{i} members, off={off} name={name_b!r} size={size}')
    off = nxt
print('total members parsed:', i)
