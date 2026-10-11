#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC="$ROOT/source"
OUT="${OUT:-$ROOT/out}"
CLANG_PATH="${CLANG_PATH:-$ROOT/toolchains/clang-r383902/bin}"
HOST_TOOLS="$ROOT/.tools/root/bin"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

[[ -f "$SRC/Makefile" ]] || { echo "Missing source tree. Run prepare-source.sh after all clones finish." >&2; exit 1; }
[[ -f "$SRC/arch/arm64/configs/X6815C_defconfig" ]] || { echo "Run scripts/prepare-source.sh first." >&2; exit 1; }
[[ -x "$CLANG_PATH/clang" ]] || { echo "Missing Android Clang at $CLANG_PATH/clang" >&2; exit 2; }
[[ -x "$HOST_TOOLS/bison" && -x "$HOST_TOOLS/flex" ]] || { echo "Missing local bison/flex host tools." >&2; exit 2; }

mkdir -p "$OUT"
export PATH="$HOST_TOOLS:$CLANG_PATH:$PATH"
export ARCH=arm64
export SUBARCH=arm64

# Pass target tools on the make command line. This old MediaTek tree assigns
# CC internally, which overrides environment-only values.
MAKE_VARS=(
  ARCH=arm64 SUBARCH=arm64
  CC=clang LD=ld.lld AR=llvm-ar NM=llvm-nm
  OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip
  HOSTCC=gcc HOSTCXX=g++
  TARGET_BUILD_VARIANT=userdebug
  CONFIG_MTK_COMBO_WIFI_HIF=axi
  MTK_COMBO_CHIP=CONNAC2X2_SOC5_0
  BT_PLATFORM=6877 WLAN_CHIP_ID=6877
  MTK_ANDROID_WMT=y MTK_ANDROID_EMI=y WIFI_IP_SET=1
  MTK_WLAN_SERVICE=yes CONNAC_VER=2_0 CFG_BUILD_CONNAC2=true
)

make -C "$SRC" O="$OUT" "${MAKE_VARS[@]}" X6815C_defconfig
make -C "$SRC" O="$OUT" "${MAKE_VARS[@]}" olddefconfig
make -C "$SRC" O="$OUT" "${MAKE_VARS[@]}" -j"$JOBS" Image.gz modules

printf '\nBuild complete. Experimental outputs:\n'
printf '  %s\n' "$OUT/arch/arm64/boot/Image.gz" "$OUT/.config"
printf '\nNo boot image was created. Do not flash this output without ABI and boot validation.\n'
