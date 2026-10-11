#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC="$ROOT/source"
REF="$ROOT/reference-prebuilt"
DEV="$ROOT/device-tree"

for p in "$SRC" "$REF" "$DEV"; do
  [[ -d "$p/.git" ]] || { echo "Missing checkout: $p" >&2; exit 1; }
done

install -m 0644 "$REF/arch/arm64/configs/X6815C_defconfig" \
  "$SRC/arch/arm64/configs/X6815C_defconfig"

DTS_DST="$SRC/arch/arm64/boot/dts/mediatek/x6815c-reference"
mkdir -p "$DTS_DST"
install -m 0644 "$REF/arch/arm64/boot/dts/mediatek/X6815C_grounded_base.dts" "$DTS_DST/"
install -m 0644 "$REF/arch/arm64/boot/dts/mediatek/X6815C_grounded_overlay_0.dts" "$DTS_DST/"
cat > "$DTS_DST/README" <<'EOF'
These files were decompiled from stock X6815C DTB/DTBO artifacts. They are
reference material, not automatically selected build inputs. Decompiled DTS
can lose source structure, labels, includes, and overlay intent. Use the stock
DTB/DTBO for initial kernel-only testing until the source hierarchy is ported
and validated node by node.
EOF

{
  echo "Generated: $(date -u +%FT%TZ)"
  echo "Baseline URL: https://github.com/xiaomi-mediatek-devs/android_kernel_xiaomi_mt6877"
  echo "Baseline branch: lineage-21"
  echo "Baseline commit: $(git -C "$SRC" rev-parse HEAD)"
  echo "Reference URL: https://github.com/rag12ghu-design/android_device_infinix_X6815C-kernel"
  echo "Reference branch: lineage-22.2"
  echo "Reference commit: $(git -C "$REF" rev-parse HEAD)"
  echo "Device URL: https://github.com/rag12ghu-design/android_device_infinix_X6815C"
  echo "Device branch: lineage-22.2"
  echo "Device commit: $(git -C "$DEV" rev-parse HEAD)"
  echo "Stock Image.gz SHA256: $(sha256sum "$REF/Image.gz" | awk '{print $1}')"
  echo "Stock DTB SHA256: $(sha256sum "$REF/dtb/mt6877.dtb" | awk '{print $1}')"
  echo "Stock DTBO SHA256: $(sha256sum "$REF/dtbo.img" | awk '{print $1}')"
} > "$ROOT/PROVENANCE.txt"

echo "Installed X6815C reference config and DTS material."
echo "Review README-X6815C.md before building or testing."
