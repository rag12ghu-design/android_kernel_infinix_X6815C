#!/usr/bin/env python3
"""
repack-boot.py - Android Boot Header v2 Repacker & AVB Signer for Infinix X6815C (Slot B Grounded)
"""

import sys
import os
import struct
import hashlib
import argparse
import subprocess

PAGE_SIZE = 2048
HEADER_VERSION = 2
KERNEL_ADDR = 0x40080000
RAMDISK_ADDR = 0x51100000
TAGS_ADDR = 0x47c80000
DTB_ADDR = 0x47c80000
CMDLINE = "bootopt=64S3,32N2,64N2 buildvariant=user"
PARTITION_SIZE = 41943040  # 40 MB

def pad_to_page(size, page_size=PAGE_SIZE):
    return ((size + page_size - 1) // page_size) * page_size

def unpack_boot(boot_path, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    with open(boot_path, "rb") as f:
        data = f.read()

    if not data.startswith(b"ANDROID!"):
        raise ValueError(f"{boot_path} is not an Android boot image")

    # Header v2: 1660 bytes
    hdr = data[:1660]
    magic, k_sz, k_addr, r_sz, r_addr, s_sz, s_addr, t_addr, pg_sz, hdr_ver = struct.unpack("<8sIIIIIIIII", hdr[:44])
    cmdline = hdr[64:64+512].decode("ascii", errors="ignore").strip("\x00")
    dtb_sz, dtb_addr = struct.unpack("<IQ", hdr[1648:1660])

    print(f"[*] Boot image header:")
    print(f"    Version: {hdr_ver}, Page size: {pg_sz}")
    print(f"    Kernel size: {k_sz} (load addr: {hex(k_addr)})")
    print(f"    Ramdisk size: {r_sz} (load addr: {hex(r_addr)})")
    print(f"    DTB size: {dtb_sz} (load addr: {hex(dtb_addr)})")
    print(f"    Cmdline: '{cmdline}'")

    k_offset = pg_sz
    r_offset = k_offset + pad_to_page(k_sz, pg_sz)
    dtb_offset = r_offset + pad_to_page(r_sz, pg_sz)

    k_data = data[k_offset:k_offset+k_sz]
    r_data = data[r_offset:r_offset+r_sz]
    dtb_data = data[dtb_offset:dtb_offset+dtb_sz]

    k_out = os.path.join(out_dir, "kernel.gz")
    r_out = os.path.join(out_dir, "ramdisk.img")
    dtb_out = os.path.join(out_dir, "dtb.img")

    with open(k_out, "wb") as f: f.write(k_data)
    with open(r_out, "wb") as f: f.write(r_data)
    with open(dtb_out, "wb") as f: f.write(dtb_data)

    print(f"[+] Extracted components to {out_dir}")
    print(f"    Kernel:  {len(k_data)} bytes -> SHA256: {hashlib.sha256(k_data).hexdigest()}")
    print(f"    Ramdisk: {len(r_data)} bytes -> SHA256: {hashlib.sha256(r_data).hexdigest()}")
    print(f"    DTB:     {len(dtb_data)} bytes -> SHA256: {hashlib.sha256(dtb_data).hexdigest()}")

def pack_boot(kernel_path, ramdisk_path, dtb_path, output_path, avbtool_path=None, cmdline=CMDLINE):
    with open(kernel_path, "rb") as f: k_data = f.read()
    with open(ramdisk_path, "rb") as f: r_data = f.read()
    with open(dtb_path, "rb") as f: dtb_data = f.read()

    k_sz = len(k_data)
    r_sz = len(r_data)
    dtb_sz = len(dtb_data)

    # Build Header v2 (1660 bytes)
    hdr = bytearray(PAGE_SIZE)
    # Magic
    hdr[0:8] = b"ANDROID!"
    # k_sz, k_addr, r_sz, r_addr
    struct.pack_into("<IIII", hdr, 8, k_sz, KERNEL_ADDR, r_sz, RAMDISK_ADDR)
    # second_size (0), second_addr (0), tags_addr, page_size, header_version (2)
    struct.pack_into("<IIIII", hdr, 24, 0, 0, TAGS_ADDR, PAGE_SIZE, HEADER_VERSION)
    # os_version (0)
    struct.pack_into("<I", hdr, 44, 0)
    # name
    name = b"CY-X6815C-H777"
    hdr[48:48+len(name)] = name
    # cmdline
    cmdline_bytes = cmdline.encode("ascii")[:512]
    hdr[64:64+len(cmdline_bytes)] = cmdline_bytes
    # header_size (1660) at offset 1644
    struct.pack_into("<I", hdr, 1644, 1660)
    # dtb_size, dtb_addr at offset 1648
    struct.pack_into("<IQ", hdr, 1648, dtb_sz, DTB_ADDR)

    # Construct unpadded image
    out = bytearray()
    out.extend(hdr)
    out.extend(k_data)
    out.extend(b"\x00" * (pad_to_page(k_sz) - k_sz))
    out.extend(r_data)
    out.extend(b"\x00" * (pad_to_page(r_sz) - r_sz))
    out.extend(dtb_data)
    out.extend(b"\x00" * (pad_to_page(dtb_sz) - dtb_sz))

    unpadded_size = len(out)
    print(f"[*] Unpadded boot image size: {unpadded_size} bytes ({unpadded_size // 1024} KB)")

    if unpadded_size > PARTITION_SIZE - 65536:
        raise ValueError(f"Boot image ({unpadded_size}) exceeds partition limit ({PARTITION_SIZE})")

    # Write temporary unpadded image
    with open(output_path, "wb") as f:
        f.write(out)

    if avbtool_path and os.path.isfile(avbtool_path):
        print("[*] Adding AVB footer using avbtool...")
        cmd = [
            sys.executable, avbtool_path, "add_hash_footer",
            "--image", output_path,
            "--partition_size", str(PARTITION_SIZE),
            "--partition_name", "boot",
            "--algorithm", "SHA256_RSA2048",
            "--key", "external/avb/testkey_rsa2048.pem" if os.path.exists("external/avb/testkey_rsa2048.pem") else "",
            "--rollback_index", "0"
        ]
        # If no key, sign with algorithm NONE or default
        if not os.path.exists("external/avb/testkey_rsa2048.pem"):
            cmd = [
                sys.executable, avbtool_path, "add_hash_footer",
                "--image", output_path,
                "--partition_size", str(PARTITION_SIZE),
                "--partition_name", "boot",
                "--rollback_index", "0"
            ]
        subprocess.run(cmd, check=True)
        print(f"[+] Successfully generated AVB-signed {output_path} ({os.path.getsize(output_path)} bytes)")
    else:
        # Pad to full partition size
        pad_len = PARTITION_SIZE - unpadded_size
        with open(output_path, "ab") as f:
            f.write(b"\x00" * pad_len)
        print(f"[+] Output padded to full partition size: {PARTITION_SIZE} bytes")

def main():
    parser = argparse.ArgumentParser(description="Infinix X6815C Boot Image Repacker & Validator")
    subparsers = parser.add_subparsers(dest="cmd", required=True)

    unpack_parser = subparsers.add_parser("unpack")
    unpack_parser.add_argument("boot_image", help="Input boot.img")
    unpack_parser.add_argument("--out", "-o", default="unpacked", help="Output directory")

    pack_parser = subparsers.add_parser("pack")
    pack_parser.add_argument("--kernel", "-k", required=True, help="Kernel Image.gz")
    pack_parser.add_argument("--ramdisk", "-r", required=True, help="Ramdisk image")
    pack_parser.add_argument("--dtb", "-d", required=True, help="Stock DTB mt6877.dtb")
    pack_parser.add_argument("--out", "-o", required=True, help="Output boot.img")
    pack_parser.add_argument("--avbtool", default="avbtool.py", help="Path to avbtool.py")
    pack_parser.add_argument("--cmdline", default=CMDLINE, help="Kernel command line")

    args = parser.parse_args()
    if args.cmd == "unpack":
        unpack_boot(args.boot_image, args.out)
    elif args.cmd == "pack":
        pack_boot(args.kernel, args.ramdisk, args.dtb, args.out, args.avbtool, args.cmdline)

if __name__ == "__main__":
    main()
