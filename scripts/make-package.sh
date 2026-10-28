#!/bin/sh
# Builds the installable Velvet Interpolator package from the template.
# Usage: scripts/make-package.sh VERSION PATH_TO_velvet_interp.so OUT_DIR
set -eu
cd "$(dirname "$0")/.."
VER="$1"; SO="$2"; OUT="$3"
case "$VER" in [0-9]*.[0-9]*.[0-9]*) ;; *) echo "error: version must look like 1.2.0" >&2; exit 1 ;; esac
[ -f "$SO" ] || { echo "error: $SO not found" >&2; exit 1; }
STAGE="$OUT/VelvetInterpolator-$VER"
rm -rf "$STAGE" && mkdir -p "$OUT" && cp -a template "$STAGE"
cp "$SO" "$STAGE/payload/vst/velvet_interp.so"
# version strings: installer, plugin registration, and the skin's version file
sed -i "s/VERSION='[0-9.]*'/VERSION='$VER'/" "$STAGE/install.sh"
sed -i "s/ version=\"[0-9.]*\"/ version=\"$VER\"/" "$STAGE/plugin.xml"
sed -i "s|<version>[0-9.]*</version>|<version>$VER.0</version>|" "$STAGE/payload/Synths/Velvet Interpolator - VST - Velvet Interpolator/version.xml"
# checksums of everything the installer copies (the installer verifies these before touching MPC)
(cd "$STAGE" && find payload -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum > SHA256SUMS)
(cd "$OUT" && rm -f "VelvetInterpolator-$VER-mpc-armv7.zip" && zip -qr "VelvetInterpolator-$VER-mpc-armv7.zip" "VelvetInterpolator-$VER")
echo "package: $OUT/VelvetInterpolator-$VER-mpc-armv7.zip"
