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

## 10. Touchscreen Driver Implementation (`nt36xxx_spi.c`)

- **Problem**: Touchscreen was completely unresponsive; no touch input devices were registered in userspace.
- **Root Cause**: `CONFIG_TOUCHSCREEN_MTK_NT36xxx_NOFLASH_COMMON=y` in defconfig only referenced an empty Kconfig stub in `drivers/misc/mediatek/tran_compat/Kconfig` with no source implementation. On Infinix X6815C, the Novatek NT36672C is a No-Flash controller wired to MT6877 SPI4 (`spi4@11018000`).
- **Resolution**:
  - Implemented standalone driver `drivers/input/touchscreen/nt36xxx_spi.c`.
  - Added volatile SRAM firmware upload streaming:
    - **ILM**: 73,095 bytes (`0x100` -> `0x00000000`)
    - **DLM**: 17,791 bytes (`0x18100` -> `0x00027140`)
  - Configured falling-edge interrupt trigger (`IRQF_TRIGGER_FALLING | IRQF_ONESHOT`) on EINT 14 (GPIO 14) and reset pin on GPIO 15.
  - Bypassed destructive hardware resets on warm boot to preserve bootloader-preloaded firmware.
  - Registered `mtk-tpd` direct multi-touch device matching Android InputReader expectations.

---

## 11. Battery Telemetry & MediaTek GM3.0 / FGADC Fallbacks

- **Problem**: Battery displayed 1% capacity and `present: false` after boot.
- **Root Cause**: `drivers/power/supply/mediatek/battery/mtk_battery.c` had an unconditional `if (!data->USE_TI_GAUGE) return -EPERM;` at lines 483-486, failing all property reads because the Infinix DTB does not set `use-ti-gauge`.
- **Resolution**:
  - Replaced unconditional `-EPERM` rejection with native fallbacks to MediaTek GM3.0 / FGADC functions (`battery_get_uisoc()`, `battery_get_bat_voltage()`, `battery_get_bat_temperature()`, `battery_get_bat_current()`, `charger_manager_get_charge_status()`).
  - Restored real-time battery percentage, voltage, temperature, and charging status across Android SystemUI.

---

## 12. MediaTek DRM Layering Rule ABI Parity

- **Problem**: Screen blanked / went black after boot logo, or severe display glitching occurred.
- **Root Cause**:
  - 64-bit userspace `/vendor/lib64/hw/hwcomposer.mt6877.so` calls IOCTL `0xc0a86445` (`_IOC(3, 'd', 0x45, 168)`), which expects `struct drm_mtk_layering_info` to contain 4 displays (168 bytes).
  - 32-bit compat userspace calls IOCTL `0xc0886445`, expecting 4 displays with 32-bit pointers (`compat_uptr_t`, 136 bytes).
  - Modifying the struct array size or alignment caused graphics composer to crash with `SIGSEGV` or reject the IOCTL with `-EINVAL`.
- **Resolution**:
  - Restored exact 168-byte struct in `include/uapi/drm/mediatek_drm.h` and 136-byte struct in `mtk_layering_rule_base.c`.
  - Relaxed `check_disp_info` validation to allow active hardware plane transitions without error.

---

## 13. Camera Subsystem: S5KJN1 Burst DMA I2C, 4-Cell Bayer Gb & ISP SOF Synchronization

- **Problem**: Camera application reported "Camera not enabled" or "Could not connect to camera"; preview had a severe magenta/pink tint and upside-down orientation.
- **Root Causes**:
  1. **Burst DMA Protocol**: Samsung S5KJN1 uses auto-incrementing SRAM address registers (`0x6028`, `0x602a`, `0x6f12`). Individual single-entry I2C writes with STOP conditions disrupted register programming, causing the sensor to output truncated lines and triggering `TG_GBERR_ST` (`0x20`) and `DMA_ERR_ST` (`0x800000`).
  2. **ISP Command Queue Desync**: When `CAM_A PHY cqcnt != VIR cqcnt`, `drivers/misc/mediatek/cameraisp/src/mt6873/camera_isp.c` explicitly stripped `SOF_INT_ST` from userspace, causing `NormalPipe` to time out with `-EFAULT` (errno 14).
  3. **Color Format**: Sensor output was configured as `Raw B` (0) instead of `SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gb` (`0x1A = 26`), swapping green and blue channels during ISP demosaicing.
  4. **Orientation**: Chassis sensor mounting is inverted, requiring hardware H+V flip (`0x0101 = 0x03`).
- **Resolution**:
  - Implemented 1016-byte burst DMA I2C transfers via `iBurstWriteReg_multi(buf, len, 0x20, 4, 1000)` in `drivers/misc/mediatek/imgsensor/src/common/v1_1/x6815c_cameras.c`.
  - Added stock hardware preamble writes (`0x6028=0x4000`, `0x0000=0x0003`, `0x0000=0x38e1`, `0x001e=0x0007`, `0x6010=0x0001`, `0x6226=0x0001`) with microsecond delays.
  - Aligned raw format to `SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gb` (`0x1A`), restoring natural color balance.
  - Programmed register `0x0101` to `0x03` and exposed `/sys/class/s5kjn1/s5kjn1_flip` for runtime adjustments.
  - Implemented feature `3094` (`SET_SHUTTER_FRAME_TIME`) for smooth 30 FPS AE lock.

---

## 14. Flashlight Sysfs Compatibility Node

- **Problem**: Stock vendor camera HAL (`libcam.hal3a.v3.platform.so`) crashed with SIGABRT due to Bionic `fclose(NULL)` when opening `/sys/class/torch/torch/torch_level`.
- **Resolution**:
  - Created `/sys/class/torch/torch/torch_level` directly in `drivers/misc/mediatek/flashlight/flashlight-core.c`.
  - Ensures stock vendor libraries open a valid file descriptor without crashing.

---

## 15. Shadow Call Stack (SCS) Architectural Decoupling

- **Problem**: Kernel panic at 16.14s during `insmod /vendor/lib/modules/bt_drv_6877.ko`.
- **Root Cause**: Stock vendor module `bt_drv_6877.ko` was compiled with `# CONFIG_SHADOW_CALL_STACK is not set`. Building the kernel with SCS enabled caused compiler to reserve register `x18` for the shadow call stack pointer and shifted `task_struct` offsets by +8 bytes, corrupting memory when the vendor module accessed `current->thread`.
- **Resolution**: Disabled `CONFIG_SHADOW_CALL_STACK` in defconfig (`# CONFIG_SHADOW_CALL_STACK is not set`), aligning ABI and resolving the crash.

---

## 16. Safe Hardware Verification Protocol

```bash
# 1. Boot device into fastboot mode
adb reboot bootloader

# 2. Flash to active boot partition (or boot_b for dual-slot testing)
fastboot flash boot out-x6815c/boot_x6815c.img

# 3. Reboot into Android
fastboot reboot
```

