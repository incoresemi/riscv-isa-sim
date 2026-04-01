#!/usr/bin/env python3
"""
Visualize a linker map file showing memory regions and section occupancy.
Usage: python3 mapviz.py memdemo.map
"""

import re
import sys

def parse_map(filename):
    regions = {}
    sections = []

    with open(filename) as f:
        lines = f.readlines()

    # Parse Memory Configuration
    in_memcfg = False
    for line in lines:
        if 'Memory Configuration' in line:
            in_memcfg = True
            continue
        if in_memcfg:
            if 'Linker script' in line:
                in_memcfg = False
                continue
            stripped = line.strip()
            if stripped == '' or stripped.startswith('Name') or stripped.startswith('*default*'):
                continue
            parts = stripped.split()
            if len(parts) >= 3 and parts[1].startswith('0x'):
                name = parts[0]
                origin = int(parts[1], 16)
                length = int(parts[2], 16)
                regions[name] = {'origin': origin, 'length': length, 'attrs': parts[3] if len(parts) > 3 else ''}

    # Parse sections — look for lines like:
    # .text           0x0000000080000000      0x3dc
    # .fast_code      0x0000000020004000       0x28 load address 0x00000000800005e8
    section_re = re.compile(r'^(\.\S+)\s+(0x[0-9a-fA-F]{8,16})\s+(0x[0-9a-fA-F]+)')
    symbol_re = re.compile(r'^\s+(0x[0-9a-fA-F]+)\s+(\S+)')

    i = 0
    while i < len(lines):
        m = section_re.match(lines[i])
        if m:
            name = m.group(1)
            addr = int(m.group(2), 16)
            size = int(m.group(3), 16)
            # Skip zero-size and metadata sections
            if size > 0 and not name.startswith('.rela') and not name.startswith('.comment') \
               and not name.startswith('.riscv') and not name.startswith('.note'):
                # Find which region this belongs to
                region = None
                for rname, rinfo in regions.items():
                    if rinfo['origin'] <= addr < rinfo['origin'] + rinfo['length']:
                        region = rname
                        break

                # Collect symbols in this section
                symbols = []
                j = i + 1
                while j < len(lines):
                    if lines[j].strip() == '' or section_re.match(lines[j]):
                        break
                    sm = symbol_re.match(lines[j])
                    if sm and not sm.group(2).startswith('*') and not sm.group(2).startswith('.') \
                       and '=' not in lines[j] and not sm.group(2).startswith('0x'):
                        sym_addr = int(sm.group(1), 16)
                        sym_name = sm.group(2)
                        if sym_addr >= addr and sym_addr < addr + size:
                            symbols.append((sym_addr, sym_name))
                    j += 1

                load_addr = None
                if 'load address' in lines[i]:
                    la = re.search(r'load address (0x[0-9a-fA-F]+)', lines[i])
                    if la:
                        load_addr = int(la.group(1), 16)

                sections.append({
                    'name': name,
                    'addr': addr,
                    'size': size,
                    'region': region,
                    'symbols': symbols,
                    'load_addr': load_addr,
                })
        i += 1

    return regions, sections


def fmt_addr(addr):
    return f"0x{addr:08X}"


def fmt_size(size):
    if size >= 1024 * 1024:
        return f"{size / (1024*1024):.1f} MB"
    elif size >= 1024:
        return f"{size / 1024:.1f} KB"
    else:
        return f"{size} B"


def bar(used, total, width=40):
    if total == 0:
        return ' ' * width
    filled = int(width * used / total)
    filled = max(filled, 1) if used > 0 else 0
    return '\u2588' * filled + '\u2591' * (width - filled)


def print_viz(regions, sections):
    # Group sections by region
    region_sections = {}
    for s in sections:
        r = s['region'] or 'UNMAPPED'
        region_sections.setdefault(r, []).append(s)

    # Sort regions by origin
    sorted_regions = sorted(regions.items(), key=lambda x: x[1]['origin'])

    # Header
    print()
    print("=" * 80)
    print("  LINKER MAP VISUALIZATION — Memory Occupancy")
    print("=" * 80)

    # Summary table
    print()
    print(f"  {'Region':<16} {'Origin':>12} {'Size':>10} {'Used':>10} {'%':>6}   Occupancy")
    print(f"  {'─' * 16} {'─' * 12} {'─' * 10} {'─' * 10} {'─' * 6}   {'─' * 40}")

    for rname, rinfo in sorted_regions:
        used = sum(s['size'] for s in region_sections.get(rname, []))
        pct = (used / rinfo['length'] * 100) if rinfo['length'] > 0 else 0
        b = bar(used, rinfo['length'])
        print(f"  {rname:<16} {fmt_addr(rinfo['origin']):>12} {fmt_size(rinfo['length']):>10} "
              f"{fmt_size(used):>10} {pct:5.1f}%   {b}")

    # Detailed view per region
    print()
    print("=" * 80)
    print("  DETAILED SECTION LAYOUT")
    print("=" * 80)

    for rname, rinfo in sorted_regions:
        secs = region_sections.get(rname, [])
        if not secs:
            continue

        total_used = sum(s['size'] for s in secs)
        pct = (total_used / rinfo['length'] * 100) if rinfo['length'] > 0 else 0

        print()
        print(f"  ┌─ {rname} ({rinfo['attrs']}) ─── {fmt_addr(rinfo['origin'])} .. "
              f"{fmt_addr(rinfo['origin'] + rinfo['length'] - 1)}  "
              f"[{fmt_size(rinfo['length'])}]")
        print(f"  │  Used: {fmt_size(total_used)} / {fmt_size(rinfo['length'])} ({pct:.1f}%)")
        print(f"  │")

        for s in sorted(secs, key=lambda x: x['addr']):
            offset = s['addr'] - rinfo['origin']
            end = s['addr'] + s['size'] - 1
            sec_bar = bar(s['size'], rinfo['length'], 30)

            load_info = ""
            if s['load_addr'] is not None:
                load_info = f"  (LMA: {fmt_addr(s['load_addr'])})"

            print(f"  │  {fmt_addr(s['addr'])} ┬─ {s['name']:<14} {fmt_size(s['size']):>8}{load_info}")
            print(f"  │  {' ' * 12}│   {sec_bar}")

            for sym_addr, sym_name in s['symbols']:
                sym_off = sym_addr - s['addr']
                print(f"  │  {' ' * 12}├── {sym_name} (+{sym_off:#x})")

            print(f"  │  {fmt_addr(end)} ┘")
            print(f"  │")

        free = rinfo['length'] - total_used
        print(f"  │  Free: {fmt_size(free)}")
        print(f"  └{'─' * 70}")

    # Copy-at-boot summary
    copies = [s for s in sections if s['load_addr'] is not None]
    if copies:
        print()
        print("=" * 80)
        print("  BOOT-TIME COPIES (startup code copies these from Flash)")
        print("=" * 80)
        print()
        print(f"  {'Section':<14} {'From (LMA)':<16} {'To (VMA)':<16} {'Size':>10}   Direction")
        print(f"  {'─' * 14} {'─' * 16} {'─' * 16} {'─' * 10}   {'─' * 30}")
        for s in copies:
            src_region = None
            for rn, ri in regions.items():
                if ri['origin'] <= s['load_addr'] < ri['origin'] + ri['length']:
                    src_region = rn
                    break
            dst_region = s['region'] or '?'
            print(f"  {s['name']:<14} {fmt_addr(s['load_addr']):<16} {fmt_addr(s['addr']):<16} "
                  f"{fmt_size(s['size']):>10}   {src_region} -> {dst_region}")

    print()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <mapfile>")
        sys.exit(1)

    regions, sections = parse_map(sys.argv[1])
    print_viz(regions, sections)
