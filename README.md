# Verified Linux 4.19.191+ Kernel Source Port for Infinix Zero 5G 2023 (X6815C)

Comprehensive, production-ready, and device-tested Linux 4.19.191+ kernel source port for the **Infinix Zero 5G 2023** (`CY-X6815C-H777` / MediaTek Dimensity 920 MT6877). Grounded on authentic Slot B factory firmware references, deep binary disassembly via Hex-Rays IDA Pro / REA, and verified live on hardware running **Android 15 HyperOS** (`Redmi/pearl/pearl:15/AP3A.240905.015.A2/OS3.0.2.0.VNQMIDC`).

> **Device-Tested • Flash-Safe • Production Ready (Build #43)**
>
> This repository contains full source implementations for the display, touchscreen, power management, camera sensors, and verified boot pipeline matching Infinix X6815C hardware.

---

## Hardware Support Status

| Subsystem | Hardware Component / Driver | Status | Details |
| :--- | :--- | :---: | :--- |
| **Display** | Novatek NT36672C 120Hz FHDP DSI (`panel-tianma-nt36672c-vdo-120hz.c`) | **WORKING** | 120Hz/90Hz/60Hz dynamic refresh, DSC line buffer alignment, clean sleep/wake sequences (`0x28`/`0x10` and `0x11`/`0x29`). |
| **Touchscreen** | Novatek NT36xxx SPI4 No-Flash (`nt36xxx_spi.c`, `touch@0`) | **WORKING** | Volatile SRAM ILM/DLM firmware streaming, MT6877 GPIO 14 (EINT) / 15 (RST), `mtk-tpd` multi-touch direct. |
| **Battery & Power** | MT6359 PMIC / MT6360 Charger (`mtk_battery.c`) | **WORKING** | MediaTek GM3.0 / FGADC native telemetry fallbacks for accurate capacity, voltage, temperature, and charging state. |
| **Keypad** | MediaTek KPD Matrix (`kpd.c`) | **WORKING** | Matrix pull-up pinctrl deferral and single-impulse anti-jam reporting for stuck hardware buttons. |
| **Primary Camera** | Samsung ISOCELL JN1 50MP (`s5kjn1_mipi_raw`, `0x38E1`) | **WORKING** | 1016-byte burst DMA I2C (`iBurstWriteReg_multi`), 4-cell Bayer Gb (`0x1A`), hardware H+V flip (`0x0101=0x03`), 30 FPS AE synchronization. |
| **Front Camera** | SK Hynix HI1631Q 16MP (`hi1631q_mipi_raw`, `0x1631`) | **WORKING** | Complete 8-mode scenario tables, 24MHz MCLK, MIPI 4-lane. |
| **Auxiliary Cameras** | GalaxyCore GC02M1 Depth (`0x02E0`) & Macro (`0x02E3`) | **WORKING** | Verified sensor probe, I2C addressing, and power-on rail sequencing. |
| **Flashlight / Torch** | Richtek / MTK Flashlight Core (`flashlight-core.c`) | **WORKING** | Compatibility sysfs node `/sys/class/torch/torch/torch_level` preventing vendor HAL `fclose(NULL)` crashes. |
| **DRM / Graphics** | MediaTek DRM Layering Rule (`mtk_layering_rule_base.c`) | **WORKING** | Strict ABI parity with 168-byte native (`0xc0a86445`) and 136-byte compat (`0xc0886445`) IOCTLs. |
| **Storage** | F2FS Filesystem (`fs/f2fs/super.c`) | **WORKING** | Transsion `tran_gc` mount option support for seamless `/data` mounting. |
| **Root & Integrity** | APatch (`KernelPatch v11282`) & AVB 1.0 Footer | **WORKING** | Built-in root support with SuperKey, strict 41,943,040-byte partition size, and AVB 1.0 hash tree. |

---

## Highlights of Architectural Fixes

### 1. Camera Sensor DMA Burst Protocol & Color Alignment
- **DMA Burst Transfers**: Samsung S5KJN1 configuration is streamed using chunked 1016-byte burst transfers via `iBurstWriteReg_multi(buf, len, 0x20, 4, 1000)`. Individual single-byte transactions with STOP conditions broke the sensor's auto-incrementing SRAM address pointers (`0x6028`, `0x602a`, `0x6f12`), triggering `TG_GBERR_ST` and `DMA_ERR_ST`.
- **MediaTek ISP SOF Handling**: Prevented hardware command queue desynchronization (`CAM_A PHY cqcnt != VIR cqcnt`) which previously caused the kernel ISP driver (`camera_isp.c`) to strip `SOF_INT_ST` from userspace, inducing `wait SOF fail` / `-EFAULT` (errno 14).
- **Color Calibration**: Configured raw output as `SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gb` (`0x1A = 26`), eliminating green/blue channel swap and magenta color cast.
- **Hardware Flip**: Programmed sensor register `0x0101` to `0x03` (H+V flip) and exposed `/sys/class/s5kjn1/s5kjn1_flip` for live orientation adjustment.

### 2. Standalone Novatek NT36xxx SPI Touchscreen Driver
- Grounded on the MT6877 SPI4 bus (`spi4@11018000`) with volatile SRAM firmware upload:
  - **ILM Partition**: File offset `0x100` -> SRAM destination `0x00000000` (73,095 bytes)
  - **DLM Partition**: File offset `0x18100` -> SRAM destination `0x00027140` (17,791 bytes)
- Correct falling-edge interrupt trigger (`IRQF_TRIGGER_FALLING | IRQF_ONESHOT`, EINT 14) and bypass of destructive hardware reset on warm boots.

### 3. DRM Layering Rule ABI Parity
- Restored exact struct sizing in `include/uapi/drm/mediatek_drm.h`:
  - **64-bit Native**: 4 displays = 168 bytes (`0xa8`) -> IOCTL `0xc0a86445`
  - **32-bit Compat**: 4 displays (`compat_uptr_t`) = 136 bytes (`0x88`) -> IOCTL `0xc0886445`
- Eliminates graphics composer crashes (`SIGSEGV` in `hwcomposer.mt6877.so`) and boot logo blanking.

### 4. Battery Monitoring Fallback Engine
- Replaced unconditional `-EPERM` rejection in `drivers/power/supply/mediatek/battery/mtk_battery.c` with native fallback to MediaTek GM3.0 / FGADC functions, restoring battery percentage, voltage, and charging state.

---

## Building the Kernel

### Prerequisites
- Ubuntu 20.04+ (WSL2 or native Linux)
- Android Clang r383902 (Clang 11.0.1) or higher
- AArch64 GCC cross-toolchain (`aarch64-linux-gnu-`)
- Python 3 with `pycryptodome` (for AVB signing)

### Compilation & Packaging
Run the master build and validation pipeline:
```bash
cd /path/to/android_kernel_infinix_X6815C
bash x6815c-port/scripts/build-and-validate.sh
```

The script compiles the kernel with ThinLTO, builds all vendor modules, repacks the Android Boot Header v2 image (`boot_a_custom.img` / `boot_x6815c.img`), and signs it with AVB 1.0 to strictly `41,943,040` bytes.

---

## Flashing Instructions

```bash
# Reboot device to fastboot mode
adb reboot bootloader

# Flash boot image to active slot
fastboot flash boot boot_x6815c.img

# Reboot into Android
fastboot reboot
```

---

## Documentation & References

- [Verified Build Fixes & Hardware Audit](x6815c-port/VERIFIED-BUILD-FIXES.md)
- [Build Pipeline & Release Notes](x6815c-port/RELEASE_NOTES.md)
- [Configuration Compatibility Matrix](x6815c-port/CONFIG-COMPATIBILITY.md)
- [Sensor & Hardware Provenance](x6815c-port/PROVENANCE.txt)
