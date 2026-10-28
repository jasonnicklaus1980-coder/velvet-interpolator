#!/bin/sh
# Gate: the binary must match what the device can load. Fails if it needs a newer glibc than 2.27
# (the 1.1.0 build's maximum) or any library other than libc, libm, libpthread, libdl and the ARM dynamic loader.
# Verified on the device: the installed Velvet Chords plugin (ARMv7-A, hard-float, VFPv4) needs libc, libpthread and libdl.
set -eu
SO="$1"
ARCH=$(readelf -h "$SO" | awk -F: '/Machine/ {print $2}')
echo "$ARCH" | grep -q "ARM" || { echo "FAIL: not an ARM binary: $ARCH" >&2; exit 1; }
readelf -A "$SO" | grep -q "Tag_CPU_arch: v7" || { echo "FAIL: not ARMv7" >&2; exit 1; }
MAXGLIBC=$(objdump -T "$SO" | grep -o 'GLIBC_[0-9.]*' | sort -V | tail -1)
echo "highest glibc symbol version: $MAXGLIBC"
[ "$(printf '%s\n%s\n' "$MAXGLIBC" GLIBC_2.27 | sort -V | tail -1)" = "GLIBC_2.27" ] || { echo "FAIL: needs $MAXGLIBC (device max is GLIBC_2.27)" >&2; exit 1; }
NEEDED=$(readelf -d "$SO" | awk '/NEEDED/ {gsub(/[\[\]]/,"",$NF); print $NF}' | sort | tr '\n' ' ')
echo "needed libraries: $NEEDED"
for lib in $NEEDED; do
    case "$lib" in libc.so.6|libm.so.6|libpthread.so.0|libdl.so.2|ld-linux-armhf.so.3) ;; *) echo "FAIL: unexpected library $lib" >&2; exit 1 ;; esac
done
echo "ARM gate passed"
