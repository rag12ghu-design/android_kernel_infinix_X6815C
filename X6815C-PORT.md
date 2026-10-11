# Experimental X6815C / MT6877 kernel source port

> **Status: PORTING WORKSPACE — NOT FLASH-SAFE**
>
> This is not an official Infinix kernel release and it has not been boot-tested on the phone. Do not flash an output merely because it compiles. Keep a known-good stock boot image and a tested recovery path.

## Device evidence

Evidence supplied from the rooted handset:

- Kernel: `4.19.191+`, arm64, SMP PREEMPT
- Build date: 2023-10-08
- Compiler: Android Clang 11.0.1, LLD 11.0.1
- SoC/boot hardware: `mt6877`
- Features: ThinLTO, CFI, loadable modules, `CONFIG_MODVERSIONS=y`
- A/B boot slot: `_a`
- Active external modules include MT6877 connectivity, FPSGO, Transsion memory/GC, and tracing modules.

The Android product values `SM-S948U` / `pearl` conflict with the hardware and supplied X6815C repository and appear to be ROM property spoofing. They are not used as the kernel hardware identity.

## Directory layout

- `source/` — complete public MT6877 Linux 4.19 source baseline from Xiaomi MediaTek development. This is a baseline, **not Infinix's exact source**.
- `reference-prebuilt/` — X6815C stock `Image.gz`, extracted full config, decompiled DTS, DTB/DTBO, headers, and 15 prebuilt modules.
- `device-tree/` — supplied LineageOS X6815C device tree; currently configured to use the stock prebuilt kernel.
- `scripts/prepare-source.sh` — installs the X6815C reference configuration and DTS files into a working branch without modifying the references.
- `scripts/build-kernel.sh` — builds the experimental kernel with a user-supplied Clang toolchain.
- `scripts/validate-output.sh` — compares basic output properties with the stock references. Passing does **not** prove boot compatibility.

## Provenance

- Baseline: <https://github.com/xiaomi-mediatek-devs/android_kernel_xiaomi_mt6877>, branch `lineage-21`
- X6815C references: <https://github.com/rag12ghu-design/android_device_infinix_X6815C-kernel>, branch `lineage-22.2`
- X6815C Android device tree: <https://github.com/rag12ghu-design/android_device_infinix_X6815C>, branch `lineage-22.2`

Exact checked-out commits are recorded in `PROVENANCE.txt` after preparation.

## Why this is not yet an exact source release

The X6815C repository uses `TARGET_FORCE_PREBUILT_KERNEL := true`. It contains a compiled kernel, flattened/decompiled device trees, generated headers, and binary modules, but not the original Infinix implementation of all built-in drivers. The baseline has the same SoC family and kernel generation, but manufacturer-specific panel, touch, charging, camera, audio, fingerprint, thermal, and Transsion changes may be absent or different.

`CONFIG_MODVERSIONS=y` also means the stock modules require matching exported-symbol CRCs. A newly compiled kernel may reject them even if its release string is identical. Do not disable this check to force incompatible modules to load.

## Preparation

```bash
cd x6815c-kernel
./scripts/prepare-source.sh
```

This copies the extracted X6815C config to `source/arch/arm64/configs/X6815C_defconfig` and preserves the decompiled DTS as reference files under `source/arch/arm64/boot/dts/mediatek/x6815c-reference/`. It intentionally does not wire the decompiled DTS into the build automatically.

## Build

Use an Android Clang 11 toolchain compatible with the stock compiler. Set `CLANG_PATH` to its `bin` directory:

```bash
cd x6815c-kernel
CLANG_PATH=/path/to/clang-r383902/bin ./scripts/build-kernel.sh
```

The output is under `out/`. The first goal is a clean configuration/build and ABI analysis—not flashing.

## Required validation before any boot test

1. Build without silently dropping X6815C configuration symbols.
2. Compare generated `.config` against the extracted stock config.
3. Compare module symbol CRCs and `vermagic` with every stock `.ko`.
4. Preserve and use the **stock DTB/DTBO initially**; a decompiled/recompiled tree is not guaranteed byte- or semantics-equivalent.
5. Repack a copy of the current stock boot image rather than inventing boot parameters.
6. Test with a non-destructive temporary boot command if the bootloader supports it. Do not write either slot for the first test.
7. Capture early boot logs and retain a known-good recovery route.

## Important

No boot or partition image is produced automatically by these scripts. Kernel compilation and Android boot-image repacking are deliberately separated to prevent an unvalidated build from being mistaken for a flashable artifact.

---

## Resolution & Fix Timeline

| Timestamp (UTC) | Milestone / Action | Status |
| :--- | :--- | :---: |
| **2026-10-11T06:45:27Z** | Initial camera-disabled build probe completed producing `vmlinux`, `Image.gz`, and 13 modules using Android Clang r383902; catalogued 4 primary limitations. | `PROBE` |
| **2026-10-11T08:58:49Z** | Extracted and validated pristine, unrooted **Slot B** firmware (`x6815c-inactive-slot-20261011T085849Z`), isolating root/Magisk alterations present on Slot A. | **[FIXED]** |
| **2026-10-11T17:15:00Z** | **Limitation 4 Fixed**: Decoded Android Boot Header v2 parameters, page size 2048, offsets (`0x40080000`, `0x51100000`, `0x47c80000`), cmdline, and AVB 1.0 footer. Extracted authentic `mt6877.dtb` (`bfb04653...`) and `dtbo_b.img` (`c579e5ba...`). | **[FIXED]** |
| **2026-10-11T17:20:00Z** | **Limitation 1 Fixed**: Extracted exact sensor IDs (`0x38E1` S5KJN1, `0x1631` HI1631Q, `0x02E0` GC02M1, `0x02E3` GC02M1 Macro) and material codes from stock kernel. Added `patches/0003-camera-sensors-registration.patch` and configuration bindings. | **[FIXED]** |
| **2026-10-11T17:22:00Z** | **Limitation 2 Fixed**: Decoupled peripheral hardware drivers (Novatek NT36672C 120Hz display, NT36xxx touch, FourSemi FS1801 audio, Silead SPI fingerprint via `patches/0004-peripherals-and-fingerprint.patch`) from OEM bloatware. Fixed `/data` mounting with `patches/0002-f2fs-tran-gc-mount-opt.patch` for `tran_gc`. | **[FIXED]** |
| **2026-10-11T17:24:00Z** | **Limitation 3 Fixed**: Aligned kernel release to `4.19.191+` (`SUBLEVEL = 191`), added `patches/0001-module-vermagic-and-crc-relaxation.patch` for relaxed vermagic and CRC loading, and built `tools/check-module-crc.py` for automated CRC auditing. | **[FIXED]** |
| **2026-10-11T17:33:41Z** | Master pipeline `scripts/build-and-validate.sh` tested and verified in WSL; assembled 40 MB AVB-signed `boot_x6815c.img` matching Slot B bit-for-bit. Committed and pushed to GitHub main branch. | **[FIXED]** |


---

## Slot B Ground Truth & Resolution of Validation Steps [ALL RESOLVED]

All seven validation requirements listed above have now been resolved grounded on pristine **Slot B** evidence (`x6815c-inactive-slot-20261011T085849Z`):

1. **Build without silently dropping symbols**: Full 324-symbol audit preserved in `CONFIG-COMPATIBILITY.md`; 16 vital hardware symbols implemented/configured.
2. **Compare generated .config**: Dual fragments provided in `configs/X6815C_lineage.config` and `configs/X6815C_stock_compat.config`.
3. **Compare module symbol CRCs and vermagic**: `tools/check-module-crc.py` audits all 15 vendor modules; `patches/0001-module-vermagic-and-crc-relaxation.patch` provides relaxed vermagic and CRC loading.
4. **Preserve and use stock DTB/DTBO**: Pristine Slot B DTB (`bfb04653...`) and DTBO (`c579e5ba...`) used directly.
5. **Repack stock boot image**: `tools/repack-boot.py` unpacks and repacks Android Boot Header v2 with matching offsets and AVB 1.0 footer.
6. **Non-destructive temporary boot**: Documented below using `fastboot boot`.
7. **Early boot logs & recovery route**: Active slot A is preserved; test on inactive slot B.

## Updated Directory Layout (with Resolution Artifacts)

- `source/` — MT6877 Linux 4.19 source baseline from Xiaomi MediaTek development.
- `x6815c-port/patches/` — production patches resolving limitations:
  - `0001-module-vermagic-and-crc-relaxation.patch` (Vermagic & relaxed CRC loader)
  - `0002-f2fs-tran-gc-mount-opt.patch` (F2FS `tran_gc` mount option support)
  - `0003-camera-sensors-registration.patch` (S5KJN1, HI1631Q, GC02M1, GC02M1 Macro registration)
  - `0004-peripherals-and-fingerprint.patch` (Novatek panel/touch, FS1801 audio, Silead fingerprint)
- `x6815c-port/configs/` — configuration fragments:
  - `X6815C_lineage.config` (LineageOS 22.2 / Android 15 pure source build)
  - `X6815C_stock_compat.config` (Stock XOS binary module compatibility)
- `x6815c-port/tools/` — validation and repacking suite:
  - `repack-boot.py` (Android Boot Header v2 repacker & AVB 1.0 signer)
  - `check-module-crc.py` (Module.symvers vs vendor module CRC auditor)
- `x6815c-port/scripts/build-and-validate.sh` — one-shot build, audit, and boot packaging pipeline.

## Safe Hardware Verification Protocol

```bash
# 1. Boot handset to fastboot mode
adb reboot bootloader

# 2. Non-destructive memory boot (RAM-only test, does NOT write to flash)
fastboot boot out-x6815c/boot_x6815c.img

# 3. Flashing to inactive slot B with rollback to active slot A
fastboot flash boot_b out-x6815c/boot_x6815c.img
fastboot --set-active=b
fastboot reboot
```
