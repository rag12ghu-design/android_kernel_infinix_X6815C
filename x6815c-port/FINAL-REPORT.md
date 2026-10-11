# X6815C / MT6877 source-port build report

## Result

The **camera-disabled build probe completed successfully** with Android Clang r383902 / 11.0.1, ThinLTO, CFI, loadable modules, and symbol versioning.

Produced:

- `vmlinux`
- `System.map`
- `arch/arm64/boot/Image`
- `arch/arm64/boot/Image.gz`
- 13 loadable modules, including MT6877 WLAN, Bluetooth, Conninfra, Connfem, FM radio, and WMT Wi-Fi adapter modules

This proves the public MT6877 source baseline can be configured and linked using an X6815C-derived configuration after documented source and probe-config corrections.

## Critical limitation

This is **not a stock-equivalent or flash-safe X6815C kernel**.

- Build-probe release: `4.19.321+`
- Stock release: `4.19.191+`
- Build-probe `Image.gz`: 13,037,816 bytes
- Stock `Image.gz`: 13,182,614 bytes
- Build-probe SHA-256: `ab587d8381ccb6553e163ddbf57c3a11815144d6a5c40e91fc167d44ee8bd999`
- Stock SHA-256: `7d9b2a13fb9bd11ca73cb086175b9eadc0b53a0e6bd94c8d942dd8597c511bd0`

The release mismatch and `CONFIG_MODVERSIONS=y` mean stock `4.19.191+` modules must not be mixed with this `4.19.321+` kernel unless every required symbol CRC is proven compatible. They are not assumed compatible.

## X6815C grounding

The workspace incorporates:

- Extracted stock X6815C kernel config
- Stock X6815C `Image.gz`, DTB, DTBO, and 15 modules
- Decompiled X6815C base DT and overlay as references
- X6815C LineageOS device tree
- X6815C vendor tree and its camera/audio/fingerprint/firmware evidence
- Live-device kernel/compiler/module information supplied from the rooted handset
- Exact stock compiler generation, Android Clang r383902 / 11.0.1

The complete compilable C baseline is the public Xiaomi-derived MT6877 Linux 4.19 tree because exact Infinix source was not present in the supplied repositories.

## Probe-only differences

The original extracted config remains preserved. Separate fragments made the build probe possible:

1. Camera integration disabled because these X6815C kernel implementations are absent:
   - `s5kjn1_mipi_raw`
   - `hi1631q_mipi_raw`
   - `gc02m1_mipi_raw`
   - `gc02m1macro_mipi_raw`
2. Baseline `MI_HARDWARE_ID` enabled to provide `get_hw_sku()` used by inherited Xiaomi power/USB/audio code.
3. GPS changed to a module to match Conninfra module linkage.
4. Goodix/FPC fingerprint disabled because vendor evidence identifies Silead and the baseline defaulted unrelated drivers on.
5. Connfem enabled as a module to satisfy stock-style WLAN/Bluetooth dependencies.

These are recorded under `configs/`.

## Source fixes

Patches under `patches/` cover:

- Forward declarations for `struct device`
- Legacy MediaTek charger header compatibility paths
- SCP ultrasound reserved-memory guard consistency
- Connfem module Kbuild correction

No missing camera, panel, touchscreen, fingerprint, or Transsion driver implementation was fabricated.

## Module result

Built modules have vermagic:

```text
4.19.321+ SMP preempt mod_unload modversions aarch64
```

Stock modules have vermagic:

```text
4.19.191+ SMP preempt mod_unload modversions aarch64
```

Connfem exports required WLAN/Bluetooth symbols after patch 0005. The probe built 13 modules; the stock reference carries 15 different device-oriented modules. Several stock Transsion modules have no public source in the baseline.

## Warnings

Module post-processing reports duplicate exports from the MT6357 and MT6359 accdet modules because equivalent symbols also exist in `vmlinux`. This needs configuration cleanup before any device test. It did not prevent module generation.

## Next safe steps

Before any device boot test:

1. Collect current-slot stock boot image and any vendor_boot image.
2. Preserve stock DTB/DTBO for the first kernel-only experiment.
3. Compare `Module.symvers` CRCs against every module required at boot.
4. Resolve accdet duplicate-export configuration.
5. Port or legally obtain the four real camera drivers and missing Transsion components.
6. Reconcile kernel version to the exact stock 4.19.191 vendor baseline if stock module compatibility is required.
7. Repack without flashing and inspect boot header, offsets, command line, ramdisk, DTB, AVB footer, and partition-size constraints.
8. Use temporary boot only if the bootloader supports it and a verified recovery route exists.

No boot image was generated, no partition was changed, and nothing has been flashed.
