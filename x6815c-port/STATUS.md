# X6815C Source-Port Status

## Current Milestone: BUILD #43 PRODUCTION RELEASE (Fully Verified & Device-Tested)

### Completed Hardware Subsystems & Drivers

- **Display**: Novatek NT36672C 120Hz/90Hz/60Hz FHDP DSI panel (`panel-tianma-nt36672c-vdo-120hz.c`) active and verified. VESA DSC line buffer aligned, clean sleep/wake sequences (`0x28`/`0x10` and `0x11`/`0x29`) without screen glitches.
- **Touchscreen**: Standalone Novatek NT36xxx SPI4 driver (`drivers/input/touchscreen/nt36xxx_spi.c`) on `spi4@11018000`. Supports volatile SRAM firmware upload (ILM: 73,095 bytes, DLM: 17,791 bytes), EINT 14 falling-edge interrupt, and `mtk-tpd` direct multi-touch.
- **Battery & Power**: MediaTek GM3.0 / FGADC fallbacks in `mtk_battery.c`, restoring real-time capacity percentage, voltage, and charging state.
- **Keypad Matrix**: `drivers/input/keyboard/mediatek/kpd.c` with pinctrl `-EPROBE_DEFER` propagation, pull-up matrix biasing, and single-impulse anti-jam reporting for stuck hardware keys.
- **Camera Subsystem**: Complete 4-sensor driver in `drivers/misc/mediatek/imgsensor/src/common/v1_1/x6815c_cameras.c`:
  - Samsung ISOCELL JN1 (50MP, `0x38E1`): 1016-byte burst DMA I2C (`iBurstWriteReg_multi`), 4-cell Bayer Gb (`0x1A`), hardware H+V flip (`0x0101=0x03`), 30 FPS AE synchronization.
  - SK Hynix HI1631Q (16MP, `0x1631`): Full 8-mode scenario tables and 24MHz MCLK.
  - GalaxyCore GC02M1 (2MP depth, `0x02E0`) & Macro (2MP, `0x02E3`): Verified I2C probe and power-on rail sequencing.
- **Flashlight**: Compatibility sysfs node `/sys/class/torch/torch/torch_level` created in `flashlight-core.c`, preventing vendor HAL libraries from crashing via Bionic `fclose(NULL)`.
- **DRM / Graphics**: MediaTek DRM layering rule ABI engine supporting 168-byte native (`0xc0a86445`) and 136-byte compat (`0xc0886445`) IOCTLs, eliminating graphics composer crashes and boot logo blanking.
- **Security & Integrity**: APatch (`KernelPatch v11282`, SuperKey: `ApatchRoot2026`) built-in; boot image repacked to strictly `41,943,040` bytes with valid AVB 1.0 signed footer.
- **Shadow Call Stack (SCS)**: Explicitly disabled (`# CONFIG_SHADOW_CALL_STACK is not set`) to resolve `bt_drv_6877.ko` `x18` clobber and `task_struct` +8-byte shift panic.

### Release Artifacts & Verification Checksums

- **`boot_x6815c.img`** (41,943,040 bytes): `166b7a91fcda6ceef16e5b952cc44edd1baf652421febfa6d5113487161396d4`
- **`Image.gz`** (13,189,877 bytes): `5bd6176e6bcd2941da7506956fc87c5cf716e128e4e2bdca0d9fd40efe129e27`
- **`X6815C-verified-boot-build43.tar.xz`** (53,354,820 bytes): `27380bb730790e694c36153223b4dc9a02d451d4b693c6b459cf18d3538f160e`
- **GitHub Release Tag**: [`v1.0.2-build43`](https://github.com/rag12ghu-design/android_kernel_infinix_X6815C/releases/tag/v1.0.2-build43)

### Safe Flashing Guidance

> [!CAUTION]
> MediaTek Little Kernel (LK) does not support `fastboot boot`. Always flash directly to a partition:
> ```bash
> adb reboot bootloader
> fastboot flash boot out-x6815c/boot_x6815c.img
> fastboot reboot
> ```
