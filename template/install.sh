#!/bin/sh
# Velvet Interpolator installer for Gen1 MPC OS devices (32-bit ARM). Run ON the device as root:   sh install.sh [-y]
# Copies velvet_interp.so to /sdcard/vst, backs up MPC.settings, registers the plugin and restarts MPC
# (MPC is stopped for the edit - save your project first). Safe to re-run: it upgrades in place.
set -e
cd "$(dirname "$0")"
NAME='Velvet Interpolator'; VERSION='0.1.0'; SO_DIR='/sdcard/vst'; SO='velvet_interp.so'; SKIN='Velvet Interpolator - VST - Velvet Interpolator'
YES=0; [ "$1" = "-y" ] && YES=1
die() { echo "error: $*" >&2; exit 1; }

# VI_TEST_ROOT: test hook that redirects all paths into a fake root and skips device checks.
PFX="${VI_TEST_ROOT:-}"
if [ -z "$PFX" ]; then
    [ "$(id -u)" = 0 ] || die "run as root"
    case "$(uname -m)" in armv7*) ;; *) die "this build is for 32-bit ARM MPC OS devices; this one is $(uname -m)" ;; esac
    command -v systemctl >/dev/null || die "systemctl not found"
fi
SETTINGS=$(ls "$PFX"/media/az01-internal/Settings/*/MPC.settings 2>/dev/null | head -n 1)
[ -n "$SETTINGS" ] || die "MPC.settings not found (not an MPC OS device?)"
[ -f "payload/vst/$SO" ] || die "payload/vst/$SO missing from the package"
[ -f "payload/Synths/$SKIN/Plugin Skins/TUI.json" ] || die "screen skin missing from the package"
if [ -f SHA256SUMS ] && command -v sha256sum >/dev/null; then
    sha256sum -c SHA256SUMS >/dev/null 2>&1 || die "package checksum mismatch; re-copy the package"
fi

echo "$NAME $VERSION will install:"
echo "  $SO_DIR/$SO, /sdcard/Synths/$SKIN (screen skin) and an entry in $SETTINGS"
grep -q '/sdcard/Synths' "$SETTINGS" || echo "note: /sdcard/Synths isn't listed in MPC's SynthContentLocations; the skin may not show"
if [ $YES = 0 ]; then
    printf "MPC will be stopped and restarted. Save your project first. Continue? [y/N] "
    read -r ok; case "$ok" in y|Y|yes) ;; *) echo "cancelled"; exit 1 ;; esac
fi

if [ -z "$PFX" ]; then
    systemctl stop acvs
    trap 'systemctl start acvs' EXIT
    i=0; while pidof MPC >/dev/null && [ $i -lt 30 ]; do sleep 1; i=$((i + 1)); done
    pidof MPC >/dev/null && die "MPC did not stop"
fi

mkdir -p "$PFX$SO_DIR"
cp "payload/vst/$SO" "$PFX$SO_DIR/$SO.new" && mv "$PFX$SO_DIR/$SO.new" "$PFX$SO_DIR/$SO"
mkdir -p "$PFX/sdcard/Synths"
rm -rf "$PFX/sdcard/Synths/$SKIN.new"; cp -a "payload/Synths/$SKIN" "$PFX/sdcard/Synths/$SKIN.new"
rm -rf "$PFX/sdcard/Synths/$SKIN"; mv "$PFX/sdcard/Synths/$SKIN.new" "$PFX/sdcard/Synths/$SKIN"

BAK="$SETTINGS.bak-velvet-interp-$(date +%Y%m%d-%H%M%S)"
cp "$SETTINGS" "$BAK"
awk -v mode=add -v file="$SO_DIR/$SO" -v entryfile=plugin.xml -f plugin_list.awk "$SETTINGS" > "$SETTINGS.new"
n=$(grep -c "file=\"$SO_DIR/$SO\"" "$SETTINGS.new" || true)
[ "$n" = 1 ] || { rm -f "$SETTINGS.new"; die "settings edit failed; MPC.settings unchanged (backup: $BAK)"; }
if command -v python3 >/dev/null; then
    python3 -c 'import sys, xml.etree.ElementTree as E; E.parse(sys.argv[1])' "$SETTINGS.new" 2>/dev/null ||
        { rm -f "$SETTINGS.new"; die "edited settings aren't valid XML; MPC.settings unchanged"; }
fi
mv "$SETTINGS.new" "$SETTINGS"
sync
echo "Installed $NAME $VERSION. Settings backup: $BAK"
echo "Add Velvet Interpolator from the plugin browser on a track or bus insert."
