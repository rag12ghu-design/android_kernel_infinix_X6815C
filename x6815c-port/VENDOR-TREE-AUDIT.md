# X6815C vendor-tree audit

Repository: `rag12ghu-design/android_vendor_infinix_X6815C`, branch `lineage-22.2`

Checked-out commit: `9bcc9db51d4ec6d899045dad7a486e5993abb235`

## Inventory

- Files in checkout: 2,610
- Kernel modules (`*.ko`): **0**
- Camera sensor/tuning files matching S5KJN1, HI1631Q, or GC02M1: 250
- Vendor firmware files: 77
- Vendor init `.rc` files: 117
- Fingerprint/Silead/fpsensor files: 14

The complete matching hardware-file list is in `vendor-hardware-files.txt`.

## What this contributes

- Camera HAL, tuning, scene, index-manager, and calibration userspace binaries for:
  - `s5kjn1_mipi_raw`
  - `hi1631q_mipi_raw`
  - `gc02m1_mipi_raw`
  - `gc02m1macro_mipi_raw`
- Camera provider and MediaTek camera HAL components.
- Silead fingerprint HAL/service and init/VINTF integration.
- FS1801 audio firmware (`firmware/fs1801.fsm`).
- MediaTek modem, connectivity, graphics, media, sensor, thermal, power, and TEE userspace components.

## What it does not contribute

The vendor tree contains no kernel modules and no kernel C implementation for the four camera sensors, panel, touchscreen, fingerprint, charger customizations, or other missing Transsion Kconfig symbols. Camera tuning `.so` files cannot replace kernel sensor drivers: the kernel still must identify, power, configure, and stream from each sensor before the HAL can use the tuning libraries.

This tree is therefore valuable for final Android integration and hardware-contract verification, but it does not turn the public MT6877 baseline into exact Infinix kernel source.
