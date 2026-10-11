#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="${OUT:-$ROOT/out}"
REF="$ROOT/reference-prebuilt"
NEW="$OUT/arch/arm64/boot/Image.gz"

[[ -f "$NEW" ]] || { echo "Missing $NEW" >&2; exit 1; }
[[ -f "$OUT/.config" ]] || { echo "Missing $OUT/.config" >&2; exit 1; }

printf '%-20s %s\n' 'stock Image.gz:' "$(stat -c %s "$REF/Image.gz") bytes"
printf '%-20s %s\n' 'new Image.gz:' "$(stat -c %s "$NEW") bytes"
printf '%-20s %s\n' 'stock sha256:' "$(sha256sum "$REF/Image.gz" | awk '{print $1}')"
printf '%-20s %s\n' 'new sha256:' "$(sha256sum "$NEW" | awk '{print $1}')"

echo
echo 'Critical generated config:'
grep -E '^(CONFIG_(ARM64|MACH_MT6877|MODULES|MODVERSIONS|LTO|THINLTO|CFI|CFI_CLANG|IKCONFIG|IKCONFIG_PROC)=|CONFIG_LOCALVERSION=|CONFIG_BUILD_SALT=)' "$OUT/.config" || true

echo
echo 'Stock modules and vermagic (when host modinfo supports them):'
for m in "$REF"/vendor-modules/*.ko; do
  if command -v modinfo >/dev/null 2>&1; then
    printf '%-30s %s\n' "$(basename "$m")" "$(modinfo -F vermagic "$m" 2>/dev/null || echo unreadable)"
  else
    echo "modinfo unavailable on host; module ABI validation remains outstanding."
    break
  fi
done

cat <<'EOF'

This is only a structural check. Different hashes are expected. A successful
build does not establish module ABI compatibility or boot safety. Preserve the
stock DTB/DTBO and validate Module.symvers CRCs before any device test.
EOF
