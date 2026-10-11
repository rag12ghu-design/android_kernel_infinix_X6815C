# Experimental Linux kernel source port for Infinix X6815C

This repository is an **experimental MT6877 Linux 4.19 source port** for the Infinix Zero 5G 2023 (`X6815C`). It is grounded with the extracted stock X6815C configuration, stock kernel/DT/module references, decompiled device-tree references, LineageOS device/vendor evidence, and live-device build information.

> **Not stock-equivalent. Not flash-safe. Not device-tested.**
>
> The complete C baseline is a public Xiaomi-derived MT6877 `4.19.321` tree because the supplied Infinix repositories contain a stock `4.19.191+` binary kernel and modules, not the complete matching kernel implementation.

## Verified build result

A camera-disabled build probe successfully produced `vmlinux`, `System.map`, `Image`, `Image.gz`, and 13 modules using Android Clang r383902 / 11.0.1 with ThinLTO, CFI, modules, and MODVERSIONS.

The build probe is deliberately not a device kernel:

- Four real X6815C camera kernel drivers are unavailable: S5KJN1, HI1631Q, GC02M1, and GC02M1 macro.
- Several Infinix/Transsion features are absent from the public baseline.
- Built release `4.19.321+` does not match stock `4.19.191+` module vermagic.
- Stock DTB/DTBO, module CRC compatibility, boot-image layout, AVB, and hardware boot remain unvalidated.

## Start here

- [Port overview and build instructions](X6815C-PORT.md)
- [Final build report](x6815c-port/FINAL-REPORT.md)
- [Exact provenance](x6815c-port/PROVENANCE.txt)
- [Configuration compatibility](x6815c-port/CONFIG-COMPATIBILITY.md)
- [Vendor-tree audit](x6815c-port/VENDOR-TREE-AUDIT.md)
- [Patches](x6815c-port/patches/)
- [Probe configuration fragments](x6815c-port/configs/)

No boot image is provided by this repository and nothing here should be flashed without completing the validation steps in the final report.
