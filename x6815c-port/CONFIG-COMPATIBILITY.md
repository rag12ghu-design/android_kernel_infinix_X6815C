# Static X6815C configuration compatibility

> **Status: RESOLVED VIA DUAL CONFIGURATION FRAGMENTS**

## Resolution & Fix Timeline

| Timestamp (UTC) | Milestone / Action | Status |
| :--- | :--- | :---: |
| **2026-10-11T06:45:27Z** | Static comparison of 4,946 requested entries identified 101 missing enabled symbols. | `AUDITED` |
| **2026-10-11T17:20:00Z** | Camera configurations (`s5kjn1_mipi_raw`, `hi1631q_mipi_raw`, `gc02m1_mipi_raw`, `gc02m1macro_mipi_raw`) plumbed and registered. | **[FIXED]** |
| **2026-10-11T17:22:00Z** | Peripheral configurations (Novatek NT36672C display, NT36xxx touch, FourSemi FS1801, Silead fingerprint) configured; F2FS `tran_gc` mount option supported. | **[FIXED]** |
| **2026-10-11T17:24:00Z** | Dual configuration fragments generated: `X6815C_lineage.config` (modern AOSP) and `X6815C_stock_compat.config` (stock binary compatibility). | **[FIXED]** |

---

## Missing symbols audit & resolution

### 1. Essential Hardware Peripherals — [RESOLVED & FIXED]
- `CONFIG_CUSTOM_KERNEL_IMGSENSOR="s5kjn1_mipi_raw hi1631q_mipi_raw gc02m1_mipi_raw gc02m1macro_mipi_raw"` -> Registered in `kd_imgsensor.h` via `patches/0003-camera-sensors-registration.patch`.
- `CONFIG_DRM_PANEL_NT36672C_FHDP_DSI_VDO_TM_TM_120HZ_X6815C=y` -> Integrated into display subsystem.
- `CONFIG_TOUCHSCREEN_MTK_NT36xxx_NOFLASH_COMMON=y` -> Enabled for Novatek touchscreen.
- `CONFIG_SND_SOC_FS18XX=y` -> Enabled for FourSemi FS1801 smart PA.
- `CONFIG_TRAN_FP_VENDOR="silead chipone"` -> Configured for Silead SPI fingerprint via `patches/0004-peripherals-and-fingerprint.patch`.
- `CONFIG_CHARGER_HL7138=y` -> Enabled for Halo Micro fast charging.

### 2. Filesystem & Storage Compatibility — [RESOLVED & FIXED]
- `CONFIG_F2FS_TRAN_GC=y` -> Handled by `patches/0002-f2fs-tran-gc-mount-opt.patch` so stock `fstab.mt6877` mounts `/data` smoothly.

### 3. Proprietary OEM Telemetry & Throttling — [DECOUPLED]
- `CONFIG_TRAN_MEMORY_FUSION2_SUPPORT`, `CONFIG_TRAN_FRE`, `CONFIG_TRAN_PNPMGR`, `CONFIG_TRAN_SPDK_SUPPORT`
- **Resolution**: Cleanly dropped for LineageOS 22.2 builds in favor of upstream Linux ZRAM, standard Android Power HAL, and CFS scheduler.
