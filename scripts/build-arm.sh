
#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

OUT="${1:-build/arm}"
CXX=arm-linux-gnueabihf-g++
CC=arm-linux-gnueabihf-gcc

command -v "$CXX" >/dev/null || {
    echo "error: $CXX not found" >&2
    exit 1
}

mkdir -p "$OUT"

FLAGS="-O2 -fPIC -fvisibility=hidden -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -Wall -Wextra"

"$CC" -std=gnu99 $FLAGS -c src/musicout.c -o "$OUT/musicout.o"
"$CC" -std=gnu99 $FLAGS -c src/chordcore.c -o "$OUT/chordcore.o"
"$CC" -std=gnu99 $FLAGS -c src/notes.c -o "$OUT/notes.o"
"$CC" -std=gnu99 $FLAGS -c src/poly.c -o "$OUT/poly.o"
"$CC" -std=gnu99 $FLAGS -c src/hpss.c -o "$OUT/hpss.o"
"$CC" -std=gnu99 $FLAGS -c src/keys.c -o "$OUT/keys.o"
"$CC" -std=gnu99 $FLAGS -c src/padsend.c -o "$OUT/padsend.o"

"$CXX" -std=c++14 $FLAGS -fno-exceptions -fno-rtti -Isrc \
    -c src/velvet_interp.cpp -o "$OUT/velvet_interp.o"

"$CXX" -shared -static-libgcc -s \
    -o "$OUT/velvet_interp.so" \
    "$OUT/velvet_interp.o" \
    "$OUT/musicout.o" \
    "$OUT/chordcore.o" \
    "$OUT/notes.o" \
    "$OUT/poly.o" \
    "$OUT/hpss.o" \
    "$OUT/keys.o" \
    "$OUT/padsend.o" \
    -lm -lpthread

echo "built $OUT/velvet_interp.so"
