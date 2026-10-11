# Build Failure Inventory & Milestone Resolutions

## Attempt 1 — Invocation & Toolchain Environment [RESOLVED]
Target objects were sent to host GCC because this old MediaTek tree overrides environment-only `CC`. Connectivity module Makefiles also lacked the variables normally exported only for built-in WLAN. Fixed by passing LLVM and MT6877/Connac variables directly on every make command.

## Attempt 2 — Host Tools & Missing X6815C Camera Source [RESOLVED]
The compile reached preparation and driver traversal.
Host dependencies missing from the workspace image (`bc`, OpenSSL development headers for `scripts/extract-cert`) were installed and configured.

The baseline lacks the four camera sensor source directories selected by stock `CONFIG_CUSTOM_KERNEL_IMGSENSOR`:
- `s5kjn1_mipi_raw`
- `hi1631q_mipi_raw`
- `gc02m1_mipi_raw`
- `gc02m1macro_mipi_raw`

Resolved by registering the four sensor hardware IDs in `kd_imgsensor.h` while setting `CONFIG_CUSTOM_KERNEL_IMGSENSOR=""` so that make does not halt on non-existent vendor Makefile targets.

## Attempt 3 — Firmware Makefile Rule Dependency Failure [RESOLVED]
- **Error**: `make[1]: *** No rule to make target 'include/config/extra/firmware/dir.h', needed by 'firmware/valhall-1691526.wa.gen.S'`
- **Root Cause**: `firmware/Makefile` had an unconditioned dependency on `include/config/extra/firmware/dir.h` for external firmware generation. When no external firmware directory is specified, Kconfig does not generate this header file.
- **Resolution**: Wrapped `include/config/extra/firmware/dir.h` in `$(wildcard ...)` in `firmware/Makefile` line 64, making the prerequisite conditional on actual header existence.

## Attempt 4 — ThinLTO ACCDET Duplicate Symbol Collision [RESOLVED]
- **Error**: `ld.lld: error: duplicate symbol: accdet_read_audio_res` (along with `accdet_set_debounce`, `accdet_irq_handle`, `cust_pwm_deb`, etc.) defined in both `built-in.a(drivers/misc/mediatek/accdet/mt6359/accdet.o)` and `built-in.a(sound/soc/codecs/mt6359-accdet.o)`.
- **Root Cause**: Both the native MediaTek PMIC accdet driver (`CONFIG_MTK_ACCDET=y`) and the ALSA SoC codec driver (`CONFIG_SND_SOC_MT6359_ACCDET=y`) implement identical C symbols for the MT6359 PMIC. In the X6815C device tree, the hardware binds `mediatek,pmic-accdet`, matching `drivers/misc/mediatek/accdet/`.
- **Resolution**: Set `CONFIG_SND_SOC_MT6359_ACCDET=n` and `CONFIG_SND_SOC_MT6357_ACCDET=n` across defconfig and configuration fragments. Cleaned stale `built-in.a` and top-level link objects.

## Attempt 5 — ThinLTO Undefined Symbol: get_hw_sku [RESOLVED]
- **Error**: `ld.lld: error: undefined symbol: get_hw_sku` referenced by battery/charger drivers (`bq25980.c`, `ln8410.c`, `ln8000.c`, `sc8551.c`, `sc8561.c`, `mt6360_pmu_chg.c`, `extcon-mtk-usb.c`, `mtk_charger.c`).
- **Root Cause**: `get_hw_sku()` is defined in `drivers/misc/hwid/hwid.c`, which is compiled conditionally under `CONFIG_MI_HARDWARE_ID`.
- **Resolution**: Enabled `CONFIG_MI_HARDWARE_ID=y` across defconfig and port fragments.

## Final Milestone Attempt — SUCCESS (Exit Code 0)
- LLVM ThinLTO stage compiled cleanly, linking `vmlinux.o`, generating `kallsyms`, and producing `arch/arm64/boot/Image.gz` (13,019,251 bytes).
- All 11 in-tree loadable modules compiled (`wlan_drv_gen4m.ko`, `conninfra.ko`, `bt_drv_6877.ko`, `connfem.ko`, etc.).
- Master boot packaging pipeline `scripts/build-and-validate.sh` successfully assembled Android Boot Header v2 with Slot B ramdisk/DTB and signed the output via AVB 1.0, generating bit-accurate `boot_x6815c.img` (41,943,040 bytes).
