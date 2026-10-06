"""
Flash audit — parse linker map OUTPUT SECTIONS only.
Validates against known output section sizes from the map header.
"""
import re, collections, sys

MAP = r'tools\build_analysis\pokedex.ino.map'

# Known output section sizes (from map header lines)
# We'll extract these programmatically
OUT_SEC_HDR = re.compile(r'^(\.\S+)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s*$')
# Input section entry within output section: two hex values + path
IN_ENTRY = re.compile(r'^\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+([^\s]+)\s*$')

def main():
    with open(MAP, encoding='utf-8', errors='replace') as f:
        lines = [l.rstrip('\n') for l in f]

    # Phase 1: Find where "Discarded input sections" ends and output sections begin
    # The discarded sections block contains entries with VMA=0x00000000
    # Output sections start with a line like: .section_name  0xADDR  0xSIZE
    
    discarded_start = None
    for i, l in enumerate(lines):
        if l.strip() == 'Discarded input sections':
            discarded_start = i
            break
    
    if discarded_start is None:
        print("ERROR: 'Discarded input sections' not found")
        sys.exit(1)
    
    # Find first output section header after discarded sections
    # Output section headers start with a dot and have a non-zero address
    out_sections_start = None
    for i in range(discarded_start + 1, len(lines)):
        m = OUT_SEC_HDR.match(lines[i])
        if m:
            out_sections_start = i
            break
    
    if out_sections_start is None:
        print("ERROR: No output section headers found")
        sys.exit(1)
    
    print(f"Discarded sections start: line {discarded_start+1}")
    print(f"Output sections start: line {out_sections_start+1}")
    print(f"Discarded section count: {out_sections_start - discarded_start - 1} lines")
    print()
    
    # Phase 2: Parse output sections
    # Track current output section
    # For each input entry, record: (output_section, size, path)
    
    # Collect output section totals (ground truth)
    out_sec_totals = {}  # section_name -> total_size
    current_out_sec = None
    
    # Collect input section entries per output section
    entries_by_out_sec = collections.defaultdict(list)  # out_sec -> [(size, path, in_sec_name)]
    
    # Also track input section names (the line before the entry)
    last_in_sec_name = None
    
    # Find end of output sections (where debug sections or symbol tables begin)
    # Debug sections have no address or have addr=0
    # We'll stop at .debug_aranges or similar
    out_sections_end = len(lines)
    for i in range(out_sections_start, len(lines)):
        if lines[i].strip() in ('.debug_aranges', '.debug_info', '.debug_abbrev', '.comment'):
            # Check if this is a section header (not an input entry)
            if OUT_SEC_HDR.match(lines[i]) or lines[i].strip().startswith('.debug') or lines[i].strip().startswith('.comment'):
                # Make sure it's a top-level section (no leading spaces)
                if not lines[i].startswith(' '):
                    out_sections_end = i
                    break
    
    print(f"Output sections end: line {out_sections_end+1}")
    print()
    
    # Now parse the output sections block
    for i in range(out_sections_start, out_sections_end):
        line = lines[i]
        
        # Check for output section header
        m = OUT_SEC_HDR.match(line)
        if m:
            sec_name = m.group(1)
            addr = int(m.group(2), 16)
            size = int(m.group(3), 16)
            current_out_sec = sec_name
            if size > 0:
                out_sec_totals[sec_name] = size
            continue
        
        # Check for input section entry (indented, two hex values, path)
        m = IN_ENTRY.match(line)
        if m and current_out_sec:
            addr = int(m.group(1), 16)
            size = int(m.group(2), 16)
            path = m.group(3)
            if size > 0 and addr != 0:  # Skip zero-size entries
                entries_by_out_sec[current_out_sec].append((size, path, last_in_sec_name))
            continue
        
        # Check for input section name (indented, just a name)
        stripped = line.strip()
        if stripped.startswith('.') and not stripped.startswith('..') and ' ' not in stripped:
            last_in_sec_name = stripped
            continue
        elif stripped.startswith('*') or stripped.startswith('SORT') or stripped.startswith('KEEP'):
            last_in_sec_name = None
            continue
    
    # Phase 3: Validate
    print("=" * 70)
    print("VALIDATION: Input section sums vs Output section sizes")
    print("=" * 70)
    
    relevant_sections = [s for s in out_sec_totals if any(
        s.startswith(p) for p in ('.flash.', '.iram0.', '.dram0.')) and out_sec_totals[s] > 0
    ]
    
    all_ok = True
    for sec in sorted(relevant_sections):
        out_size = out_sec_totals[sec]
        in_sum = sum(e[0] for e in entries_by_out_sec.get(sec, []))
        diff = out_size - in_sum
        pct = (in_sum / out_size * 100) if out_size > 0 else 0
        status = "OK" if abs(diff) / max(out_size, 1) < 0.05 else "CHECK"
        if status == "CHECK":
            all_ok = False
        print(f"  {sec:25s} out={out_size:>10d}  in_sum={in_sum:>10d}  diff={diff:>+8d}  ({pct:.1f}%) {status}")
    
    print()
    
    # Phase 4: Aggregate by archive/object for flash sections
    flash_sections = [s for s in out_sec_totals if s.startswith('.flash.')]
    
    per_archive = collections.Counter()
    per_object = collections.Counter()
    
    for sec in flash_sections:
        for size, path, in_sec in entries_by_out_sec.get(sec, []):
            # Extract archive name
            # Path format: C:\...\libxxx.a(member.o) or C:\...\file.cpp.o
            pm = re.search(r'([A-Za-z0-9_]+\.a)\(([^)]+)\)', path)
            if pm:
                archive = pm.group(1)
                member = pm.group(2)
            else:
                # Just a .o file
                archive = None
                member = None
            
            # Get filename
            filename = path.replace('\\', '/').split('/')[-1]
            
            if archive:
                per_archive[archive] += size
            per_object[filename] += size
    
    # Also include non-archive objects grouped by directory
    per_source = collections.Counter()
    for sec in flash_sections:
        for size, path, in_sec in entries_by_out_sec.get(sec, []):
            pm = re.search(r'([A-Za-z0-9_]+\.a)\(([^)]+)\)', path)
            if pm:
                key = pm.group(1)
            else:
                # Determine source from path
                p = path.replace('\\', '/')
                if '/sketch/' in p:
                    key = '[sketch]'
                elif '/libraries/' in p:
                    lib = p.split('/libraries/')[1].split('/')[0]
                    key = f'[lib:{lib}]'
                elif '/core/' in p:
                    key = '[core]'
                else:
                    key = p.split('/')[-1]
            per_source[key] += size
    
    print("=" * 70)
    print(f"FLASH SECTIONS: {', '.join(flash_sections)}")
    print(f"TOTAL FLASH: {sum(out_sec_totals[s] for s in flash_sections):,} bytes")
    print("=" * 70)
    print()
    
    print("TOP 25 BY SOURCE (archives + sketch + core):")
    for name, size in per_source.most_common(25):
        print(f"  {name:50s} {size:>10,} bytes  ({size/1024:.1f} KB)")
    print()
    
    print("TOP 30 BY OBJECT FILE:")
    for name, size in per_object.most_common(30):
        print(f"  {name:60s} {size:>10,} bytes  ({size/1024:.1f} KB)")
    print()
    
    # Summary
    total_flash = sum(out_sec_totals[s] for s in flash_sections)
    app_size = 0x140000  # from partition table
    fw_size = 1264144  # from .bin file
    
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"  Flash physical:      16 MB")
    print(f"  Partition scheme:    default (4MB layout)")
    print(f"  APP partition:       0x140000 = {app_size:,} bytes ({app_size/1024/1024:.2f} MB)")
    print(f"  Firmware binary:     {fw_size:,} bytes ({fw_size/1024/1024:.3f} MB)")
    print(f"  APP utilization:     {fw_size/app_size*100:.1f}%")
    print(f"  Margin:              {app_size - fw_size:,} bytes ({(app_size-fw_size)/1024:.1f} KB)")
    print()
    print(f"  If using default_16MB.csv:")
    app16 = 0x640000
    print(f"    APP partition:     0x640000 = {app16:,} bytes ({app16/1024/1024:.2f} MB)")
    print(f"    APP utilization:   {fw_size/app16*100:.1f}%")
    print(f"    Margin:            {app16 - fw_size:,} bytes ({(app16-fw_size)/1024/1024:.2f} MB)")

if __name__ == '__main__':
    main()
