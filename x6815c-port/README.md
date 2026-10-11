# X6815C Source-Port Support Files & Build Pipeline

This directory contains the patches, configuration fragments, build scripts, audit reports, and provenance for the verified Infinix Zero 5G 2023 (X6815C / MT6877) Linux 4.19 kernel source port.

## Key Documentation Index
- [`FINAL-REPORT.md`](FINAL-REPORT.md): Comprehensive source-port build and verification report.
- [`VERIFIED-BUILD-FIXES.md`](VERIFIED-BUILD-FIXES.md): In-depth technical breakdown of all 4 resolved limitations, ThinLTO linker resolutions, and AVB 1.0 signing.
- [`STATUS.md`](STATUS.md): Current verified milestone status and safe hardware testing instructions.
- [`RELEASE_NOTES.md`](RELEASE_NOTES.md): Official release notes for GitHub release `v1.0.0-slot-b`.
- [`CONFIG-COMPATIBILITY.md`](CONFIG-COMPATIBILITY.md): 443-symbol Kconfig audit and Transsion compatibility mapping.
- [`VENDOR-TREE-AUDIT.md`](VENDOR-TREE-AUDIT.md): Complete vendor tree and hardware evidence inventory.
- [`BUILD-FAILURES.md`](BUILD-FAILURES.md): Post-mortem of all build attempts and final resolutions.

## Quick Build & Packaging Instructions

```bash
# Full build and packaging pipeline
bash x6815c-port/scripts/build-and-validate.sh
```

Output:
- Boot image: `out-x6815c/boot_x6815c.img` (41,943,040 bytes, AVB 1.0 signed)
- Compressed kernel: `out-x6815c/arch/arm64/boot/Image.gz` (13 MB)

## Safe Hardware Verification

```bash
# 1. Boot to bootloader
adb reboot bootloader

# 2. Non-destructive RAM test (does not write to NAND storage)
fastboot boot out-x6815c/boot_x6815c.img

# 3. Flashing to inactive Slot B (guarantees Slot A recovery rollback)
fastboot flash boot_b out-x6815c/boot_x6815c.img
fastboot --set-active=b
fastboot reboot
```
