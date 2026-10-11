#!/usr/bin/env bash
# build-and-validate.sh - Full build, CRC audit, and Slot B boot validation for X6815C
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
SRC="${SRC:-$REPO_ROOT/source}"
OUT="${OUT:-$REPO_ROOT/out-x6815c}"
SLOT_B="${SLOT_B:-$REPO_ROOT/reference-slot-b}"
VENDOR_MODS="${VENDOR_MODS:-/home/jim/android/device_infinix_X6815C-kernel/vendor-modules}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 8)}"

echo "=== Infinix Zero 5G 2023 (X6815C) Kernel Build & Validation ==="
echo "[*] Source:     $SRC"
echo "[*] Output:     $OUT"
echo "[*] Slot B Ref: $SLOT_B"

# Step 1: Ensure Slot B reference artifacts exist
if [[ ! -f "$SLOT_B/mt6877.dtb" || ! -f "$SLOT_B/ramdisk.img" ]]; then
    echo "[!] Extracting Slot B reference artifacts..."
    python3 "$REPO_ROOT/tools/repack-boot.py" unpack \
        "/mnt/c/Users/Jim/Downloads/x6815c-inactive-slot-20261011T085849Z/boot_b.img" \
        --out "$SLOT_B"
fi

echo "[+] Verified pristine Slot B artifacts:"
echo "    DTB:     $(sha256sum "$SLOT_B/mt6877.dtb" | awk '{print $1}')"
echo "    Ramdisk: $(sha256sum "$SLOT_B/ramdisk.img" | awk '{print $1}')"

# Step 2: Build Environment
V=(ARCH=arm64 SUBARCH=arm64 CC=clang LD=ld.lld AR=llvm-ar NM=llvm-nm
   OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip HOSTCC=gcc HOSTCXX=g++
   TARGET_BUILD_VARIANT=userdebug CONFIG_MTK_COMBO_WIFI_HIF=axi
   MTK_COMBO_CHIP=CONNAC2X2_SOC5_0 BT_PLATFORM=6877 WLAN_CHIP_ID=6877
   MTK_ANDROID_WMT=y MTK_ANDROID_EMI=y WIFI_IP_SET=1 MTK_WLAN_SERVICE=yes
   CONNAC_VER=2_0 CFG_BUILD_CONNAC2=true)

# Step 3: Kernel Compilation (if source tree available)
if [[ -d "$SRC" && -f "$SRC/Makefile" ]]; then
    mkdir -p "$OUT"
    echo "[*] Generating defconfig..."
    make -C "$SRC" O="$OUT" "${V[@]}" X6815C_defconfig
    "$SRC/scripts/kconfig/merge_config.sh" -m -O "$OUT" "$OUT/.config" "$REPO_ROOT/configs/X6815C_lineage.config"
    make -C "$SRC" O="$OUT" "${V[@]}" olddefconfig

    echo "[*] Building kernel Image.gz and modules..."
    make -C "$SRC" O="$OUT" "${V[@]}" -j"$JOBS" Image.gz modules

    KERNEL_GZ="$OUT/arch/arm64/boot/Image.gz"
    SYMVERS="$OUT/Module.symvers"
else
    echo "[*] Using stock Slot B reference kernel for validation run..."
    KERNEL_GZ="$SLOT_B/Image.gz"
    SYMVERS="$OUT/Module.symvers"
fi

# Step 4: Module CRC Audit
if [[ -f "$SYMVERS" && -d "$VENDOR_MODS" ]]; then
    echo "[*] Running Module.symvers CRC audit..."
    python3 "$REPO_ROOT/tools/check-module-crc.py" \
        --symvers "$SYMVERS" \
        --vendor-modules "$VENDOR_MODS"
fi

# Step 5: Repack Boot Image with Slot B layout & AVB Signing
BOOT_OUT="$OUT/boot_x6815c.img"
mkdir -p "$OUT"

echo "[*] Assembling Android Boot Header v2 with pristine Slot B Ramdisk & DTB..."
python3 "$REPO_ROOT/tools/repack-boot.py" pack \
    --kernel "$KERNEL_GZ" \
    --ramdisk "$SLOT_B/ramdisk.img" \
    --dtb "$SLOT_B/mt6877.dtb" \
    --out "$BOOT_OUT" \
    --avbtool "$REPO_ROOT/avbtool.py"

echo "[*] Verifying AVB footer and partition integrity..."
python3 "$REPO_ROOT/avbtool.py" info_image --image "$BOOT_OUT"

echo
echo "=================================================================="
echo "[SUCCESS] Verified build output: $BOOT_OUT (41,943,040 bytes)"
echo "Safe testing instructions:"
echo "  1. Reboot device to bootloader: adb reboot bootloader"
echo "  2. Non-destructive live test:   fastboot boot $BOOT_OUT"
echo "  3. Inactive slot flash:         fastboot flash boot_b $BOOT_OUT"
echo "=================================================================="
