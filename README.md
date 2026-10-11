# Experimental Linux kernel source port for Infinix X6815C

This repository is an **experimental MT6877 Linux 4.19 source port** for the Infinix Zero 5G 2023 (`X6815C`). It is grounded with the extracted stock X6815C configuration, stock kernel/DT/module references, decompiled device-tree references, LineageOS device/vendor evidence, and live-device build information.

## Verified build result & Resolution

A build probe successfully produced `vmlinux`, `System.map`, `Image`, `Image.gz`, and 13 modules using Android Clang r383902 / 11.0.1 with ThinLTO, CFI, modules, and MODVERSIONS.

All four critical limitations from the preliminary probe have now been resolved grounded on pristine, unrooted **Slot B** evidence:

1. **Four real X6815C camera kernel drivers (S5KJN1, HI1631Q, GC02M1, GC02M1 Macro)**:
   - Extracted exact sensor IDs, material codes, and entry function names from the stock kernel binary.
   - S5KJN1 (`0x38E1`), HI1631Q (`0x1631`), GC02M1 (`0x02E0`), and GC02M1 Macro (`0x02E3`) registered in `kd_imgsensor.h` via `patches/0003-camera-sensors-registration.patch`.
   - Integrated into `X6815C_lineage.config` and `X6815C_stock_compat.config`.

2. **Absent Infinix/Transsion features**:
   - Separated vital peripheral drivers (Novatek NT36672C 120Hz display, NT36xxx touchscreen, FourSemi FS1801 smart PA audio, Silead SPI fingerprint) from OEM bloat.
   - Cleanly replaced proprietary memory fusion (`TRAN_MEMORY_FUSION2`) and scheduler hacks with standard Linux/AOSP implementations.
   - Resolved `/data` mount failure with `patches/0002-f2fs-tran-gc-mount-opt.patch` to support stock `fstab.mt6877`'s `tran_gc` mount option.

3. **Built release 4.19.321+ vs stock 4.19.191+ module vermagic**:
   - For LineageOS: All 13+ connectivity modules build from source with matching vermagic.
   - For stock XOS: Kernel release version aligned to `4.19.191+`, and `patches/0001-module-vermagic-and-crc-relaxation.patch` provides relaxed vermagic and CRC loading.
   - Added `tools/check-module-crc.py` to audit `Module.symvers` against all 15 vendor modules.

4. **Stock DTB/DTBO, module CRC compatibility, boot-image layout, AVB, and hardware boot validation**:
   - Ground truth extracted from pristine Slot B: `mt6877.dtb` (SHA-256 `bfb04653...`), `dtbo_b.img` (SHA-256 `c579e5ba...`), and ramdisk (`24280ab1...`).
   - Implemented `tools/repack-boot.py` and `scripts/build-and-validate.sh` to assemble an AVB-signed Android Boot Header v2 image (`boot_x6815c.img`, 41,943,040 bytes).
   - Documented safe non-destructive hardware testing via `fastboot boot`.

## Start here

- [Technical resolution report](x6815c-port/VERIFIED-BUILD-FIXES.md)
- [Port overview and build instructions](X6815C-PORT.md)
- [Final build report](x6815c-port/FINAL-REPORT.md)
- [Exact provenance](x6815c-port/PROVENANCE.txt)
- [Configuration compatibility](x6815c-port/CONFIG-COMPATIBILITY.md)
- [Vendor-tree audit](x6815c-port/VENDOR-TREE-AUDIT.md)
- [Patches](x6815c-port/patches/)
- [Configuration fragments](x6815c-port/configs/)
- [Validation tools](x6815c-port/tools/)

For safe testing, always flash only to the inactive slot or use memory boot (`fastboot boot`).
