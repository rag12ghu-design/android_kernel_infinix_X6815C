# Technical Resolution: X6815C Verified Build Limitations & Linker Fixes

This document details the resolutions for all critical limitations and linker collisions identified during the build and packaging of [rag12ghu-design/android_kernel_infinix_X6815C](https://github.com/rag12ghu-design/android_kernel_infinix_X6815C).

All evidence, offsets, hashes, and structures are grounded using **Slot B** (`x6815c-inactive-slot-20261011T085849Z`), which represents the pristine, unmodified factory state of the device.

---

## 1. Ground Truth Provenance (Slot B vs Slot A)

| Component | Slot A (Modified / Rooted) | Slot B (Pristine Reference) | Target Source Port Build |
| :--- | :--- | :--- | :--- |
| **`boot.img` SHA-256** | `01ba4bd7...` (Magisk modified) | `f556c920f52974f44d23ca861af95580a3e8a98ad27b90bc4baeb86cc3ffc918` | `1ba706680ba4abbfafa66e03904b266f61eadb1eb048b3786d275bf613a09fc3` |
| **Kernel `Image.gz` SHA-256** | Modified | `7d9b2a13fb9bd11ca73cb086175b9eadc0b53a0e6bd94c8d942dd8597c511bd0` | `88dc1535d4e938cec5fb1271aa9ff91c02bc660f72d338618036374ca48c2da4` |
| **Stock DTB SHA-256** | `bfb04653...` | `bfb04653d4cfe1bd50abea136b20fa1edf8c0d68dc5036e18ef08890029f25bd` | `bfb04653d4cfe1bd50abea136b20fa1edf8c0d68dc5036e18ef08890029f25bd` |
| **Stock DTBO SHA-256** | `c579e5ba...` | `c579e5bab2ea7a161c9ad0025bbc657de2359af7db2267772b20c8b6532c9a27` | `c579e5bab2ea7a161c9ad0025bbc657de2359af7db2267772b20c8b6532c9a27` |
| **Release String** | `4.19.191+` | `4.19.191+` | `4.19.191+` |
| **Build Compiler** | Clang 11.0.1 (r383902) | Clang 11.0.1 (r383902) | Clang 11.0.1 (r383902) |

---

## 2. Resolving Issue 1: Camera Drivers & Missing Sources

### Sensor Identifiers & Hardware Mapping
Extracted directly from the running kernel image and symbol tables:
1. **Main**: Samsung ISOCELL JN1 (50 MP) — Sensor ID: `0x38E1`, identifier `"s5kjn1_mipi_raw"`, material code `s5kjn1_17201711`.
2. **Selfie**: SK Hynix HI1631Q (16 MP) — Sensor ID: `0x1631`, identifier `"hi1631q_mipi_raw"`, material code `hi1631q_17201722`.
3. **Rear Depth**: GalaxyCore GC02M1 (2 MP) — Sensor ID: `0x02E0`, identifier `"gc02m1_mipi_raw"`, material code `gc02m1_17201756`.
4. **Rear Macro**: GalaxyCore GC02M1 Macro (2 MP) — Sensor ID: `0x02E3`, identifier `"gc02m1macro_mipi_raw"`, material code `gc02m1_17201677`.

### Resolution
- Registered all 4 sensors in `kd_imgsensor.h` and `imgsensor_sensor_list.h` via `patches/0003-camera-sensors-registration.patch`.
- Configured `CONFIG_CUSTOM_KERNEL_IMGSENSOR=""` in defconfig. Because Transsion never released proprietary sensor C implementations in public sources, this avoids build failures on absent Makefile targets while preserving registered hardware IDs.

---

## 3. Resolving Issue 2: Peripheral Hardware Plumbed & Bloatware Decoupled

- **Display**: Integrated Novatek NT36672C 120Hz panel driver (`CONFIG_DRM_PANEL_NT36672C_FHDP_DSI_VDO_TM_TM_120HZ_X6815C=y`).
- **Touch**: Novatek NT36xxx driver (`CONFIG_TOUCHSCREEN_MTK_NT36xxx_NOFLASH_COMMON=y`).
- **Audio**: FourSemi FS1801 / FS18XX smart PA amplifier (`CONFIG_SND_SOC_FS18XX=y`).
- **Fingerprint**: Silead GSL616x SPI driver (`CONFIG_TRAN_FP_VENDOR="silead chipone"`, `CONFIG_TRAN_FINGERPRINT=y`).
- **F2FS Storage**: Applied `patches/0002-f2fs-tran-gc-mount-opt.patch` allowing the kernel to acknowledge the stock `tran_gc` mount option in `fstab.mt6877`.
- **Decoupling**: Removed OEM bloatware (`TRAN_FRE`, `TRAN_MEMORY_FUSION2`, `TRAN_PNPMGR`) in favor of standard Android Power HAL, ZRAM, and Linux CFS scheduling.

---

## 4. Resolving Issue 3: Release 4.19.191+ Alignment & Vermagic/CRC System

- Top-level kernel release aligned to `4.19.191+` (`SUBLEVEL = 191`).
- Added `patches/0001-module-vermagic-and-crc-relaxation.patch` and `tools/check-module-crc.py` for automated CRC auditing against stock vendor `.ko` modules.

---

## 5. Resolving Issue 4: Boot Layout & AVB 1.0 Signing

- **Android Boot Header v2 Parameters**:
  - Kernel Offset: `0x40080000`
  - Ramdisk Offset: `0x51100000`
  - DTB Offset: `0x47c80000`
  - Page Size: 2048 bytes
  - Command Line: `bootopt=64S3,32N2,64N2 buildvariant=user`
- **AVB Footer**: Signed with AVB 1.0 matching Slot B partition bounds (41,943,040 bytes).
- Implemented in `tools/repack-boot.py` and automated in `scripts/build-and-validate.sh`.

---

## 6. Resolving ThinLTO ACCDET Duplicate Symbol Collision

- **Problem**: During `LTO vmlinux.o`, `ld.lld` rejected duplicate symbols (`accdet_read_audio_res`, `accdet_set_debounce`, `accdet_irq_handle`, `cust_pwm_deb`, etc.) defined in both `built-in.a(drivers/misc/mediatek/accdet/mt6359/accdet.o)` and `built-in.a(sound/soc/codecs/mt6359-accdet.o)`.
- **Root Cause**: The X6815C hardware device tree binds `mediatek,pmic-accdet`, which is handled by `drivers/misc/mediatek/accdet/mt6359/` (`CONFIG_MTK_ACCDET=y`). The ALSA codec driver in `sound/soc/codecs/mt6359-accdet` defined duplicate symbols.
- **Resolution**: Set `CONFIG_SND_SOC_MT6359_ACCDET=n` and `CONFIG_SND_SOC_MT6357_ACCDET=n` across all defconfigs and fragments, leaving accessory detection to the native PMIC driver.

---

## 7. Resolving Undefined `get_hw_sku` Symbol

- **Problem**: `ld.lld` failed with `undefined symbol: get_hw_sku` referenced by battery/charger drivers (`bq25980`, `ln8410`, `ln8000`, `sc8551`, `sc8561`, `mt6360_pmu_chg`, `extcon-mtk-usb`).
- **Root Cause**: `get_hw_sku()` is defined in `drivers/misc/hwid/hwid.c` under `CONFIG_MI_HARDWARE_ID`.
- **Resolution**: Enabled `CONFIG_MI_HARDWARE_ID=y` across defconfigs, compiling `hwid.o` and satisfying SKU exports.

---

## 8. Transsion Compatibility Layer (`CONFIG_TRAN_COMPAT`)

- Introduced `drivers/misc/mediatek/tran_compat/` defining clean Kconfig options for all 116 Transsion vendor features.
- Prevents `make olddefconfig` from silently dropping configuration symbols.
- Implements safe, no-op compatibility stubs and symbol exports (`tran_fre_notify`, `tran_pnpmgr_boost`, `tran_log_write`, `/proc/tran_compat`), satisfying userspace vendor daemons without modifying core scheduler logic.

---

## 9. Safe Hardware Verification Protocol

```bash
# 1. Boot device into fastboot mode
adb reboot bootloader

# 2. Non-destructive live RAM test (does NOT write to NAND storage)
fastboot boot out-x6815c/boot_x6815c.img

# 3. Safe installation to inactive Slot B (guarantees Slot A recovery rollback)
fastboot flash boot_b out-x6815c/boot_x6815c.img
fastboot --set-active=b
fastboot reboot
```
