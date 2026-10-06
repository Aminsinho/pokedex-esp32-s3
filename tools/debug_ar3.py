import sys, os, subprocess, re
sys.path.insert(0, r'tools')
import ar_extract

OBJDUMP = r'C:\Users\aminj\AppData\Local\Arduino15\packages\esp32\tools\s3-gcc\2021r2-p5\bin\xtensa-esp32s3-elf-objdump.exe'
TMP = os.path.join(os.environ.get('TEMP', r'C:\Windows\Temp'), 'ardeb')
os.makedirs(TMP, exist_ok=True)

def measure(arch, member):
    out = os.path.join(TMP, member.replace('/', '_').replace('\\','_') + '.bin')
    ar_extract.extract_member(arch, member, out)
    r = subprocess.run([OBJDUMP, '-h', out], capture_output=True, text=True)
    tot = 0
    for line in r.stdout.splitlines():
        m = re.match(r'^\s*\d+:\s+(\S+)\s+0*\w+\s+0*\w+\s+(0x[0-9a-f]+|0+)\s', line)
        if not m: continue
        name = m.group(1)
        try: size = int(m.group(3), 16)
        except (ValueError, TypeError): continue
        if name.startswith(('.text', '.literal', '.rodata', '.data', '.bss')):
            tot += size
    os.remove(out)
    return tot

A = r'C:\Users\aminj\AppData\Local\Arduino15\packages\esp32\tools\esp32s3-libs\3.3.11\lib\libnet80211.a'
members, data = ar_extract.list_members(A)
print('total raw members:', len(members))
real = [(n,o,s) for n,o,s in members if not n.startswith('/')]
meta = [(n,o,s) for n,o,s in members if n.startswith('/')]
print('real .o members:', len(real), ' meta(/N) members:', len(meta))
print()
print('meta members:')
for n,o,s in meta:
    print(f'  {n!r:12} size={s}')
print()
# Inspect one meta member's content
n, o, s = meta[3]
print(f'--- first 100 bytes of {n!r}:')
print(repr(data[o:o+100]))
print()
# measure all real members
tot = 0
for n, o, s in real:
    sz = measure(A, n)
    tot += sz
    print(f'  {n:40} raw={s:8d}  code+ro+data={sz:8d}')
print(f'TOTAL real members: {tot}')
print(f'Sum of ALL (incl meta raw sizes): {sum(s for _,_,s in members)}')
