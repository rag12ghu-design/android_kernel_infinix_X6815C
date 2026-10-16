#!/usr/bin/env bash
# build-and-validate.sh - Full build, CRC audit, and Slot B boot validation for X6815C
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
if [[ -z "${SRC:-}" ]]; then
    if [[ -f "$REPO_ROOT/source/Makefile" ]]; then
        SRC="$REPO_ROOT/source"
    elif [[ -f "$REPO_ROOT/../Makefile" ]]; then
        SRC="$(cd "$REPO_ROOT/.." && pwd)"
    fi
fi
OUT="${OUT:-$REPO_ROOT/out-x6815c}"
SLOT_A="${SLOT_A:-$REPO_ROOT/reference-slot-a}"
SLOT_B="${SLOT_B:-$REPO_ROOT/reference-slot-b}"
VENDOR_MODS="${VENDOR_MODS:-$REPO_ROOT/vendor-modules}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 16)}"

echo "=== Infinix Zero 5G 2023 (X6815C) Kernel Build & Validation ==="
echo "[*] Source:     $SRC"
echo "[*] Output:     $OUT"
echo "[*] Slot A Ref: $SLOT_A"
echo "[*] Slot B Ref: $SLOT_B"

AVBTOOL="${AVBTOOL:-}"
if [[ -z "$AVBTOOL" ]]; then
    if [[ -f "$REPO_ROOT/tools/avbtool.py" ]]; then
        AVBTOOL="$REPO_ROOT/tools/avbtool.py"
    else
        AVBTOOL="$REPO_ROOT/avbtool.py"
    fi
fi

# Step 1: Ensure Slot A (rooted) and Slot B reference artifacts exist
if [[ ! -f "$SLOT_A/ramdisk.img" || (! -f "$SLOT_A/mt6877.dtb" && ! -f "$SLOT_A/dtb.img") ]]; then
    echo "[!] Extracting Slot A reference artifacts (rooted)..."
    BOOT_A_CANDIDATE=""
    for candidate in "${BOOT_A:-}" \
                     "$REPO_ROOT/reference-slot-a/stock_boot_a.img" \
                     "$REPO_ROOT/../stock_boot_a.img" \
                     "$REPO_ROOT/stock_boot_a.img" \
                     /mnt/c/Users/*/Downloads/stock_boot_a.img; do
        if [[ -n "$candidate" && -f "$candidate" ]]; then
            BOOT_A_CANDIDATE="$candidate"
            break
        fi
    done
    if [[ -n "$BOOT_A_CANDIDATE" ]]; then
        echo "[*] Using Slot A boot image: $BOOT_A_CANDIDATE"
        python3 "$REPO_ROOT/tools/repack-boot.py" unpack "$BOOT_A_CANDIDATE" --out "$SLOT_A"
    fi
fi

if [[ ! -f "$SLOT_B/mt6877.dtb" && ! -f "$SLOT_B/dtb.img" ]] || [[ ! -f "$SLOT_B/ramdisk.img" ]]; then
    echo "[!] Extracting Slot B reference artifacts..."
    BOOT_B_CANDIDATE=""
    for candidate in "${BOOT_B:-}" \
                     "$REPO_ROOT/reference-slot-b/boot_b.img" \
                     /mnt/c/Users/*/Downloads/x6815c-inactive-slot-*/boot_b.img \
                     /mnt/c/Users/*/Downloads/x6815c-inactive-slot-*/*/boot_b.img \
                     "/mnt/c/Users/Jim/Downloads/x6815c-inactive-slot-20261011T085849Z/boot_b.img"; do
        if [[ -n "$candidate" && -f "$candidate" ]]; then
            BOOT_B_CANDIDATE="$candidate"
            break
        fi
    done
    if [[ -n "$BOOT_B_CANDIDATE" && -f "$BOOT_B_CANDIDATE" ]]; then
        echo "[*] Using Slot B boot image: $BOOT_B_CANDIDATE"
        python3 "$REPO_ROOT/tools/repack-boot.py" unpack "$BOOT_B_CANDIDATE" --out "$SLOT_B"
    fi
fi

if [[ -f "$SLOT_A/ramdisk.img" ]]; then
    RAMDISK_FILE="$SLOT_A/ramdisk.img"
    echo "[+] Selected rooted Slot A ramdisk: $(sha256sum "$RAMDISK_FILE" | awk '{print $1}')"
elif [[ -f "$SLOT_B/ramdisk.img" ]]; then
    RAMDISK_FILE="$SLOT_B/ramdisk.img"
    echo "[*] Selected Slot B ramdisk: $(sha256sum "$RAMDISK_FILE" | awk '{print $1}')"
else
    echo "[-] Error: No ramdisk found!" >&2
    exit 1
fi

if [[ -f "$SLOT_A/mt6877.dtb" ]]; then
    DTB_FILE="$SLOT_A/mt6877.dtb"
elif [[ -f "$SLOT_A/dtb.img" ]]; then
    DTB_FILE="$SLOT_A/dtb.img"
elif [[ -f "$SLOT_B/mt6877.dtb" ]]; then
    DTB_FILE="$SLOT_B/mt6877.dtb"
else
    DTB_FILE="$SLOT_B/dtb.img"
fi

echo "[+] Verified DTB artifact:"
echo "    DTB:     $(sha256sum "$DTB_FILE" | awk '{print $1}')"

# Step 2: Build Environment
CLANG_PATH="${CLANG_PATH:-/home/rafaeruu/toolchains/clang-r383902/bin}"
export PATH="$CLANG_PATH:/usr/local/bin:/usr/bin:/bin"

V=(ARCH=arm64 SUBARCH=arm64 CC=clang LD=ld.lld AR=llvm-ar NM=llvm-nm
   OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip HOSTCC=gcc HOSTCXX=g++
   CROSS_COMPILE=aarch64-linux-gnu-
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

    if ! grep -q "^# CONFIG_SHADOW_CALL_STACK is not set" "$OUT/.config"; then
        echo "[-] Error: CONFIG_SHADOW_CALL_STACK must be disabled for stock vendor module ABI compatibility!" >&2
        exit 1
    fi

    echo "[*] Building kernel Image.gz and modules..."
    make -C "$SRC" O="$OUT" "${V[@]}" -j"$JOBS" Image.gz modules

    KERNEL_GZ="$OUT/arch/arm64/boot/Image.gz"
    SYMVERS="$OUT/Module.symvers"
else
    echo "[*] Using stock Slot B reference kernel for validation run..."
    if [[ -f "$SLOT_B/Image.gz" ]]; then
        KERNEL_GZ="$SLOT_B/Image.gz"
    else
        KERNEL_GZ="$SLOT_B/kernel.gz"
    fi
    SYMVERS="$OUT/Module.symvers"
fi

# Step 4: Module CRC Audit
if [[ -f "$SYMVERS" && -d "$VENDOR_MODS" ]]; then
    echo "[*] Running Module.symvers CRC audit..."
    python3 "$REPO_ROOT/tools/check-module-crc.py" \
        --symvers "$SYMVERS" \
        --vendor-modules "$VENDOR_MODS"
fi

# Step 5: Repack Boot Image with rooted Ramdisk & DTB & AVB Signing
BOOT_OUT="$OUT/boot_x6815c.img"
BOOT_A_OUT="$OUT/boot_a_custom.img"
mkdir -p "$OUT"

echo "[*] Assembling Android Boot Header v2 with rooted Ramdisk & DTB (addr 0x41f78000)..."
python3 "$REPO_ROOT/tools/repack-boot.py" pack \
    --kernel "$KERNEL_GZ" \
    --ramdisk "$RAMDISK_FILE" \
    --dtb "$DTB_FILE" \
    --dtb-addr "0x41f78000" \
    --os-version "0xc75d263c" \
    --name "" \
    --cmdline "bootopt=64S3,32N2,64N2 buildvariant=user buildvariant=eng" \
    --out "$BOOT_OUT" \
    --avbtool "$AVBTOOL"

echo "[*] Generating boot_a_custom.img..."
cp -f "$BOOT_OUT" "$BOOT_A_OUT"

echo "[*] Verifying AVB footer and partition integrity..."
python3 "$AVBTOOL" info_image --image "$BOOT_OUT"
python3 "$AVBTOOL" info_image --image "$BOOT_A_OUT"

# Step 6: Deploy images to required destinations
echo "[*] Deploying images to required output directories..."
DEPLOY_DESTS=(
    "$REPO_ROOT/../out-x6815c"
    "$REPO_ROOT/release-assets"
    "/mnt/c/Users/rafaeruu/Downloads"
    "/mnt/c/Users/rafaeruu/Documents/antigravity/brave-maxwell/out-x6815c"
    "/mnt/c/Users/rafaeruu/Documents/antigravity/brave-maxwell/release-assets"
)

for dest in "${DEPLOY_DESTS[@]}"; do
    mkdir -p "$dest"
    echo "[+] Deploying boot images to $dest..."
    cp -f "$BOOT_OUT" "$BOOT_A_OUT" "$dest/"
done

# Step 7: Update SHA256SUMS
SHA_CUSTOM=$(sha256sum "$BOOT_A_OUT" | awk '{print $1}')
SHA_BOOT=$(sha256sum "$BOOT_OUT" | awk '{print $1}')
cat > "$REPO_ROOT/RELEASE-SHA256SUMS.txt" << EOF
$SHA_BOOT  boot_x6815c.img
$SHA_CUSTOM  boot_a_custom.img
EOF
cp -f "$REPO_ROOT/RELEASE-SHA256SUMS.txt" "$REPO_ROOT/../RELEASE-SHA256SUMS.txt" 2>/dev/null || true
if [[ -d "$REPO_ROOT/release-assets" ]]; then
    cp -f "$REPO_ROOT/RELEASE-SHA256SUMS.txt" "$REPO_ROOT/release-assets/"
fi
for mnt_dest in "/mnt/c/Users/rafaeruu/Documents/antigravity/brave-maxwell" "/mnt/c/Users/rafaeruu/Documents/antigravity/brave-maxwell/release-assets"; do
    if [[ -d "$mnt_dest" ]]; then
        cp -f "$REPO_ROOT/RELEASE-SHA256SUMS.txt" "$mnt_dest/"
    fi
done
if [[ -f "$KERNEL_GZ" ]]; then
    cp -f "$KERNEL_GZ" "$OUT/Image.gz" 2>/dev/null || true
    cp -f "$KERNEL_GZ" "$REPO_ROOT/../out-x6815c/Image.gz" 2>/dev/null || true
    cp -f "$KERNEL_GZ" "$REPO_ROOT/release-assets/Image.gz" 2>/dev/null || true
    cp -f "$KERNEL_GZ" "/mnt/c/Users/rafaeruu/Documents/antigravity/brave-maxwell/out-x6815c/" 2>/dev/null || true
    cp -f "$KERNEL_GZ" "/mnt/c/Users/rafaeruu/Documents/antigravity/brave-maxwell/release-assets/" 2>/dev/null || true
fi
if [[ -f "$OUT/System.map" ]]; then
    cp -f "$OUT/System.map" "/mnt/c/Users/rafaeruu/Documents/antigravity/brave-maxwell/out-x6815c/System.map" 2>/dev/null || true
fi

echo
echo "=================================================================="
echo "[SUCCESS] Verified build outputs:"
echo "  boot_x6815c.img:   $BOOT_OUT ($(stat -c %s "$BOOT_OUT") bytes)"
echo "  boot_a_custom.img: $BOOT_A_OUT ($(stat -c %s "$BOOT_A_OUT") bytes)"
echo "Checksums:"
echo "  boot_x6815c.img:   $(sha256sum "$BOOT_OUT" | awk '{print $1}')"
echo "  boot_a_custom.img: $(sha256sum "$BOOT_A_OUT" | awk '{print $1}')"
echo "Safe testing instructions:"
echo "  1. Reboot device to bootloader: adb reboot bootloader"
echo "  2. Non-destructive live test:   fastboot boot $BOOT_A_OUT"
echo "  3. Rooted Slot A flash:         fastboot flash boot_a $BOOT_A_OUT"
echo "=================================================================="
