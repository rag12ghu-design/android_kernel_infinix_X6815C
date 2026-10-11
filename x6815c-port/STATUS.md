# X6815C source-port status

## Completed

- Complete public MT6877 Linux 4.19 baseline cloned.
- X6815C reference/prebuilt repository and LineageOS device tree cloned.
- Extracted X6815C config and decompiled DTS references integrated.
- Exact stock Android Clang r383902 / 11.0.1 installed and verified.
- X6815C Kconfig generation succeeds under the stock compiler with MT6877, ThinLTO, CFI, modules, and MODVERSIONS enabled.
- Camera-disabled build probe completed successfully producing `vmlinux`, `System.map`, `Image`, `Image.gz`, and 13 modules.
- **Pristine Slot B ground truth integration completed** (`x6815c-inactive-slot-20261011T085849Z`), isolating unrooted stock binaries.
- **Camera drivers resolved**: Sensor IDs (`0x38E1` S5KJN1, `0x1631` HI1631Q, `0x02E0` GC02M1, `0x02E3` GC02M1 Macro), entry points, and registration patch `patches/0003-camera-sensors-registration.patch` added.
- **Peripheral & storage fixes**: Novatek NT36672C display, NT36xxx touch, FourSemi FS1801 audio, Silead SPI fingerprint configured; F2FS `tran_gc` mount option patch `patches/0002-f2fs-tran-gc-mount-opt.patch` added.
- **Vermagic & CRC compatibility**: Version aligned to `4.19.191+`, `patches/0001-module-vermagic-and-crc-relaxation.patch` added, and `tools/check-module-crc.py` built for module auditing.
- **Boot image repacker & AVB signing**: `tools/repack-boot.py` and `scripts/build-and-validate.sh` built and tested, assembling verified 40 MB AVB-signed boot image with Header v2 and stock Slot B DTB and ramdisk.

## Current state

- Grounded on unmodified Slot B stock firmware.
- Dual configuration fragments available:
  - `configs/X6815C_lineage.config` (modern pure-source Android 15 / LineageOS 22.2)
  - `configs/X6815C_stock_compat.config` (stock XOS 12/13 binary module compatibility)
- Full documentation and technical resolution in `VERIFIED-BUILD-FIXES.md`.

## Safe testing protocol

- Never flash untested builds directly to active slot.
- Use non-destructive memory boot: `fastboot boot out-x6815c/boot_x6815c.img`.
- Flash only to inactive slot B with rollback path to active slot A.
