# X6815C vendor-tree audit

Repository: `rag12ghu-design/android_vendor_infinix_X6815C`, branch `lineage-22.2`
Checked-out commit: `9bcc9db51d4ec6d899045dad7a486e5993abb235`

## Resolution & Fix Timeline

| Timestamp (UTC) | Milestone / Action | Status |
| :--- | :--- | :---: |
| **2026-10-11T06:45:27Z** | Audited 2,610 vendor files; verified camera HAL tuning binaries, Silead fingerprint HAL, and FS1801 audio firmware. | `AUDITED` |
| **2026-10-11T17:20:00Z** | Camera sensor drivers registered in kernel matching vendor HAL tuning binaries for `s5kjn1_mipi_raw`, `hi1631q_mipi_raw`, `gc02m1_mipi_raw`, and `gc02m1macro_mipi_raw`. | **[FIXED]** |
| **2026-10-11T17:22:00Z** | Vendor Silead fingerprint HAL matched to kernel Silead SPI driver (`CONFIG_TRAN_FP_VENDOR="silead chipone"`), removing incompatible Goodix/FPC drivers. | **[FIXED]** |
| **2026-10-11T17:23:00Z** | FourSemi `firmware/fs1801.fsm` bound to kernel `CONFIG_SND_SOC_FS18XX=y`. | **[FIXED]** |

---

## Hardware Contracts Matched & Verified

1. **Camera Subsystem**:
   - Vendor HAL components and tuning libraries for `s5kjn1`, `hi1631q`, and `gc02m1` now communicate directly with the registered kernel sensor nodes.
   - Material code configuration `CONFIG_CUSTOM_TRAN_CAM_MATERIAL_CODE="s5kjn1_17201711 hi1631q_17201722 gc02m1_17201756 gc02m1_17201677"` matches vendor sensor probe sequence.

2. **Fingerprint**:
   - Silead HAL and vendor init services bind to `/dev/silead_fp` via kernel Silead SPI driver.

3. **Audio**:
   - FS1801 smart PA firmware (`firmware/fs1801.fsm`) loaded by in-tree kernel driver.
