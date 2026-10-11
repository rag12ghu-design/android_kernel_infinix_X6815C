#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC="$ROOT/source"
OUT="${OUT:-$ROOT/out-probe}"
CLANG="$ROOT/toolchains/clang-r383902/bin"
TOOLS="$ROOT/.tools/root"
JOBS="${JOBS:-8}"
export PATH="$TOOLS/bin:$CLANG:$PATH"
export PKG_CONFIG_PATH="$TOOLS/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export HOSTCFLAGS="-I$TOOLS/include ${HOSTCFLAGS:-}"
export HOSTLDFLAGS="-L$TOOLS/lib -Wl,-rpath,$TOOLS/lib ${HOSTLDFLAGS:-}"
V=(ARCH=arm64 SUBARCH=arm64 CC=clang LD=ld.lld AR=llvm-ar NM=llvm-nm
   OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip HOSTCC=gcc HOSTCXX=g++
   TARGET_BUILD_VARIANT=userdebug CONFIG_MTK_COMBO_WIFI_HIF=axi
   MTK_COMBO_CHIP=CONNAC2X2_SOC5_0 BT_PLATFORM=6877 WLAN_CHIP_ID=6877
   MTK_ANDROID_WMT=y MTK_ANDROID_EMI=y WIFI_IP_SET=1 MTK_WLAN_SERVICE=yes
   CONNAC_VER=2_0 CFG_BUILD_CONNAC2=true
   HOSTCFLAGS="$HOSTCFLAGS" HOSTLDFLAGS="$HOSTLDFLAGS")
[[ -f "$OUT/.config" ]] || { echo "Run build-probe.sh first" >&2; exit 1; }
make -C "$SRC" O="$OUT" "${V[@]}" -j"$JOBS" Image.gz modules
