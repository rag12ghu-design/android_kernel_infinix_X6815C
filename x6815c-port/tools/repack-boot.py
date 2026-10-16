#!/usr/bin/env python3
"""
repack-boot.py - Android Boot Header v2 Repacker & AVB Signer for Infinix X6815C (Slot A Rooted & Slot B)
"""

import sys
import os
import struct
import hashlib
import json
import argparse
import subprocess

PAGE_SIZE = 2048
HEADER_VERSION = 2
KERNEL_ADDR = 0x40080000
RAMDISK_ADDR = 0x51100000
TAGS_ADDR = 0x47c80000
DTB_ADDR = 0x41f78000
OS_VERSION = 0xc75d263c
NAME = ""
CMDLINE = "bootopt=64S3,32N2,64N2 buildvariant=user buildvariant=eng"
PARTITION_SIZE = 41943040  # 40 MB

DEFAULT_AVB_PROPS = [
    "com.android.build.boot.fingerprint:INFINIX/twrp_X6815C/X6815C:99.87.36/SP2A.220405.004/runner06021117:eng/test-keys",
    "com.android.build.boot.os_version:99.87.36",
    "com.android.build.boot.security_patch:2099-12-31",
]

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
    os_ver = struct.unpack("<I", hdr[44:48])[0]
    name = hdr[48:64].split(b"\x00")[0].decode("ascii", errors="ignore")
    cmdline = hdr[64:64+512].decode("ascii", errors="ignore").strip("\x00")
    id_sha = hdr[576:608].hex()
    dtb_sz, dtb_addr = struct.unpack("<IQ", hdr[1648:1660])

    print(f"[*] Boot image header:")
    print(f"    Version: {hdr_ver}, Page size: {pg_sz}")
    print(f"    Kernel size: {k_sz} (load addr: {hex(k_addr)})")
    print(f"    Ramdisk size: {r_sz} (load addr: {hex(r_addr)})")
    print(f"    DTB size: {dtb_sz} (load addr: {hex(dtb_addr)})")
    print(f"    OS Version: {hex(os_ver)}")
    print(f"    Name: '{name}'")
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
    with open(os.path.join(out_dir, "Image.gz"), "wb") as f: f.write(k_data)
    with open(r_out, "wb") as f: f.write(r_data)
    with open(dtb_out, "wb") as f: f.write(dtb_data)
    with open(os.path.join(out_dir, "mt6877.dtb"), "wb") as f: f.write(dtb_data)

    meta = {
        "header_version": hdr_ver,
        "page_size": pg_sz,
        "kernel_addr": hex(k_addr),
        "ramdisk_addr": hex(r_addr),
        "tags_addr": hex(t_addr),
        "dtb_addr": hex(dtb_addr),
        "os_version": hex(os_ver),
        "name": name,
        "cmdline": cmdline,
        "id_sha": id_sha,
    }
    with open(os.path.join(out_dir, "boot_info.json"), "w") as f:
        json.dump(meta, f, indent=2)

    print(f"[+] Extracted components and metadata to {out_dir}")
    print(f"    Kernel:  {len(k_data)} bytes -> SHA256: {hashlib.sha256(k_data).hexdigest()}")
    print(f"    Ramdisk: {len(r_data)} bytes -> SHA256: {hashlib.sha256(r_data).hexdigest()}")
    print(f"    DTB:     {len(dtb_data)} bytes -> SHA256: {hashlib.sha256(dtb_data).hexdigest()}")

def pack_boot(kernel_path, ramdisk_path, dtb_path, output_path, avbtool_path=None,
              cmdline=CMDLINE, dtb_addr=DTB_ADDR, os_version=OS_VERSION, name=NAME,
              props=None):
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
    # os_version
    struct.pack_into("<I", hdr, 44, os_version)
    # name (up to 16 bytes, padded with zeros)
    name_bytes = name.encode("ascii")[:16]
    hdr[48:48+len(name_bytes)] = name_bytes
    # cmdline (up to 512 bytes)
    cmdline_bytes = cmdline.encode("ascii")[:512]
    hdr[64:64+len(cmdline_bytes)] = cmdline_bytes
    # id (SHA-1 of kernel + ramdisk + dtb)
    h = hashlib.sha1()
    h.update(k_data)
    h.update(r_data)
    h.update(dtb_data)
    digest = h.digest()
    hdr[576:576+len(digest)] = digest
    # header_size (1660) at offset 1644
    struct.pack_into("<I", hdr, 1644, 1660)
    # dtb_size, dtb_addr at offset 1648
    struct.pack_into("<IQ", hdr, 1648, dtb_sz, dtb_addr)

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
        print("[*] Adding AVB footer with full stock property descriptors...")
        cmd = [
            sys.executable, avbtool_path, "add_hash_footer",
            "--image", output_path,
            "--partition_size", str(PARTITION_SIZE),
            "--partition_name", "boot",
            "--rollback_index", "0"
        ]
        
        avb_props = DEFAULT_AVB_PROPS if props is None else props
        for p in avb_props:
            cmd.extend(["--prop", p])

        if os.path.exists("external/avb/testkey_rsa2048.pem"):
            cmd.extend(["--algorithm", "SHA256_RSA2048", "--key", "external/avb/testkey_rsa2048.pem"])

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
    pack_parser.add_argument("--dtb-addr", default=hex(DTB_ADDR), help="DTB load address (hex)")
    pack_parser.add_argument("--os-version", default=hex(OS_VERSION), help="OS version packed int (hex)")
    pack_parser.add_argument("--name", default=NAME, help="Boot image name field (16 bytes max)")
    pack_parser.add_argument("--prop", action="append", default=None, help="AVB property descriptor KEY:VALUE")

    args = parser.parse_args()
    if args.cmd == "unpack":
        unpack_boot(args.boot_image, args.out)
    elif args.cmd == "pack":
        dtb_addr = int(args.dtb_addr, 0) if isinstance(args.dtb_addr, str) else args.dtb_addr
        os_version = int(args.os_version, 0) if isinstance(args.os_version, str) else args.os_version
        pack_boot(args.kernel, args.ramdisk, args.dtb, args.out, args.avbtool,
                  cmdline=args.cmdline, dtb_addr=dtb_addr, os_version=os_version,
                  name=args.name, props=args.prop)

if __name__ == "__main__":
    main()
