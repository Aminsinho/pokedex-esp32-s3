#!/usr/bin/env python3
"""Extract archive members by parsing the BSD ar format manually (ar p seems buggy)."""
import struct, sys, os

def list_members(path):
    with open(path, 'rb') as f:
        data = f.read()
    assert data[:8] == b'!<arch>\n', 'not an ar archive'
    off = 8
    members = []
    while off + 68 <= len(data):
        name_b = data[off:off+16].decode('ascii', 'replace').strip()
        # BSD convention: 16th byte '/' means name is exactly 15 chars (slash-terminated)
        if len(name_b) > 2 and name_b.endswith('/') and not name_b.endswith('//'):
            name_b = name_b[:-1]
        # ar layout: name(16) mtime(12) uid(6) gid(6) mode(8) size(10) magic(2)
        size_s = data[off+48:off+58].decode('ascii', 'replace').strip()
        magic = data[off+58:off+60]
        if magic[:1] != b'`':
            break
        size = int(size_s)
        members.append((name_b, off+60, size))
        off += 60 + size
        off = (off + 1) & ~1  # 2-byte alignment
    return members, data

def main():
    arch, member = sys.argv[1], sys.argv[2]
    out = sys.argv[3]
    members, data = list_members(arch)
    for name, off, size in members:
        if name == member or name.endswith('/' + member) or name.endswith('\\' + member):
            with open(out, 'wb') as f:
                f.write(data[off:off+size])
            print(f"extracted {member}: {size} bytes @ {off}")
            return 0
    print(f"member {member} NOT FOUND. Available:")
    for name, off, size in members[:60]:
        print(f"  {size:10d}  {name}")
    return 1

if __name__ == '__main__':
    sys.exit(main())
