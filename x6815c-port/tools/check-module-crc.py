#!/usr/bin/env python3
"""
check-module-crc.py - Audits Module.symvers against stock vendor module imported CRCs
"""

import os
import sys
import struct
import argparse

def parse_module_versions(ko_path):
    with open(ko_path, "rb") as f:
        elf = f.read()

    if elf[:4] != b"\x7fELF":
        return {}

    # Read 64-bit ELF sections
    e_shoff = struct.unpack("<Q", elf[40:48])[0]
    e_shentsize = struct.unpack("<H", elf[58:60])[0]
    e_shnum = struct.unpack("<H", elf[60:62])[0]
    e_shstrndx = struct.unpack("<H", elf[62:64])[0]

    shstr_hdr = elf[e_shoff + e_shstrndx * e_shentsize : e_shoff + (e_shstrndx + 1) * e_shentsize]
    shstr_off = struct.unpack("<Q", shstr_hdr[24:32])[0]

    sections = {}
    for i in range(e_shnum):
        hdr = elf[e_shoff + i * e_shentsize : e_shoff + (i + 1) * e_shentsize]
        sh_name_idx = struct.unpack("<I", hdr[0:4])[0]
        sh_offset = struct.unpack("<Q", hdr[24:32])[0]
        sh_size = struct.unpack("<Q", hdr[32:40])[0]
        name = elf[shstr_off + sh_name_idx : elf.find(b"\x00", shstr_off + sh_name_idx)].decode("ascii", errors="ignore")
        sections[name] = (sh_offset, sh_size)

    if "__versions" not in sections:
        return {}

    off, sz = sections["__versions"]
    data = elf[off:off+sz]
    
    syms = {}
    pos = 0
    while pos + 64 <= sz:
        crc = struct.unpack("<Q", data[pos:pos+8])[0]
        name = data[pos+8:pos+64].split(b"\x00")[0].decode("ascii", errors="ignore")
        if name:
            syms[name] = crc
        pos += 64

    return syms

def load_symvers(symvers_path):
    syms = {}
    if not os.path.isfile(symvers_path):
        return syms

    with open(symvers_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 2:
                # Format: CRC \t Symbol \t Module/vmlinux \t Export_type
                try:
                    crc = int(parts[0], 16)
                    sym = parts[1]
                    syms[sym] = crc
                except ValueError:
                    continue
    return syms

def audit(symvers_path, vendor_modules_dir):
    symvers = load_symvers(symvers_path)
    print(f"[*] Loaded {len(symvers)} symbols from {symvers_path}")

    total_modules = 0
    clean_modules = 0
    modules = [f for f in os.listdir(vendor_modules_dir) if f.endswith(".ko")]

    for m in sorted(modules):
        total_modules += 1
        m_path = os.path.join(vendor_modules_dir, m)
        req_syms = parse_module_versions(m_path)
        
        mismatches = []
        missing = []
        matches = 0

        for s, req_crc in req_syms.items():
            if s not in symvers:
                missing.append(s)
            elif symvers[s] != req_crc:
                mismatches.append((s, req_crc, symvers[s]))
            else:
                matches += 1

        print(f"\n[-] {m}: {len(req_syms)} imported symbols")
        print(f"    Matched: {matches}, Mismatched: {len(mismatches)}, Missing in symvers: {len(missing)}")

        if mismatches:
            print("    Sample CRC mismatches:")
            for s, req, cur in mismatches[:5]:
                print(f"      {s:<30} Required: {hex(req)}  Current: {hex(cur)}")

        if not mismatches and not missing:
            clean_modules += 1
            print("    [PASS] 100% compatible with kernel ABI")

    print("\n" + "="*60)
    print(f"Summary: {clean_modules}/{total_modules} stock modules 100% CRC-compatible.")

def main():
    parser = argparse.ArgumentParser(description="Audit Module.symvers vs stock modules")
    parser.add_argument("--symvers", "-s", required=True, help="Path to built Module.symvers")
    parser.add_argument("--vendor-modules", "-v", required=True, help="Directory containing stock *.ko modules")
    args = parser.parse_args()

    audit(args.symvers, args.vendor_modules)

if __name__ == "__main__":
    main()
