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
