# X6815C source-port status

## Completed

- Complete public MT6877 Linux 4.19 baseline cloned.
- X6815C reference/prebuilt repository and LineageOS device tree cloned.
- Extracted X6815C config and decompiled DTS references integrated without overwriting provenance.
- Exact stock Android Clang r383902 / 11.0.1 installed and verified.
- Local persistent host tools built: GNU m4 1.4.19, Bison 3.8.2, Flex 2.6.4, bc 7.0.3.
- X6815C Kconfig generation succeeds under the stock compiler with MT6877, ThinLTO, CFI, modules, and MODVERSIONS enabled.
- Static and resolved configuration compatibility reports generated.
- Build invocation corrected for old MediaTek makefiles and MT6877 Connac module parameters.
- Build reached MediaTek driver traversal and confirmed missing X6815C camera source implementations.

## Current findings

- Live kernel: Linux `4.19.191+`.
- Public baseline: Linux `4.19.321`, commit `e01b96d7bb9312eb87f88d0fba118c26a0fc3484`.
- This is a full MT6877 source-port workspace, not exact unpublished Infinix source.
- Stock config has 4,946 entries. The public baseline loses 344 after Kconfig resolution, including 116 enabled settings.
- Confirmed missing camera source directories: `s5kjn1_mipi_raw`, `hi1631q_mipi_raw`, `gc02m1_mipi_raw`, and `gc02m1macro_mipi_raw`.
- A separate build probe disables camera integration only to inventory further gaps; the extracted stock config remains unchanged.
- A local OpenSSL 1.1.1 development build is still running for the kernel host utility `scripts/extract-cert`.

## Not completed

- Camera-disabled build probe beyond preparation.
- Complete missing-source inventory.
- Full kernel compilation.
- Module symbol CRC comparison.
- Device-tree source port and validation.
- Boot image repacking or hardware boot test.

## Safety state

**No flashable or bootable image has been produced. Do not flash any file in this directory.**
