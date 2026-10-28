#!/bin/sh
# Host tests: C music output, monophonic note extraction, the plugin driven by a fake host (chords, line,
# edge cases, transposition).
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -c src/musicout.c -o build/musicout.o
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -c src/chordcore.c -o build/chordcore.o
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -c src/notes.c -o build/notes.o
g++ -std=c++14 -O2 -Wall -Wextra -DVI_OUT_DIR=\"build/out\" -Isrc -c src/velvet_interp.cpp -o build/velvet_interp.o
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -c src/poly.c -o build/poly.o
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -c src/hpss.c -o build/hpss.o
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -c src/keys.c -o build/keys.o
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -o build/test_keys tests/test_keys.c src/keys.c -lm
./build/test_keys | tail -1
g++ -std=c++14 -O2 -Wall -Wextra -Isrc -c tests/host_test.cpp -o build/host_test.o
g++ -o build/host_test build/velvet_interp.o build/host_test.o build/musicout.o build/chordcore.o build/notes.o build/poly.o build/hpss.o build/keys.o -lm -lpthread
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -o build/test_musicout tests/test_musicout.c build/musicout.o build/chordcore.o -lm
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -o build/test_notes tests/test_notes.c build/notes.o -lm
./build/test_musicout > /dev/null || { echo "FAIL: music output"; exit 1; }
echo "music output tests passed"
./build/test_notes > /dev/null || { echo "FAIL: note tests"; exit 1; }
echo "note tests passed"
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -o build/test_stream tests/test_stream.c build/chordcore.o -lm
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -c src/poly.c -o build/poly.o
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -c src/hpss.c -o build/hpss.o
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -c src/keys.c -o build/keys.o
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -o build/test_keys tests/test_keys.c src/keys.c -lm
./build/test_keys | tail -1
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc -o build/test_poly tests/test_poly.c build/poly.o -lm
for cfg in "2024 4 1.0" "777 5 1.0" "4242 4 1.5"; do
  ./build/test_poly $cfg > build/poly.log
  f1=$(sed -n 's/.*F1 \([0-9.]*\)%.*/\1/p' build/poly.log)
  echo "polyphonic F1 ($cfg): $f1%"
  awk -v f="$f1" 'BEGIN { exit !(f >= 75.0) }' || { echo "FAIL: polyphonic F1 below 75% for $cfg"; exit 1; }
done
./build/test_stream > /dev/null || { echo "FAIL: stream tests"; exit 1; }
echo "stream tests passed"
g++ -std=c++14 -O2 -Wall -Wextra -Isrc -c tests/live_test.cpp -o build/live_test.o
g++ -o build/live_test build/live_test.o build/velvet_interp.o build/musicout.o build/chordcore.o build/notes.o build/poly.o build/hpss.o build/keys.o -lm -lpthread
rm -rf build/out && ./build/live_test && test -f build/out/progression_A.mid && test -f build/out/voiced_chords.mid && echo "live mode wrote progression_A.mid and voiced_chords.mid" || (echo "FAIL: live mode did not write the chord MIDI files"; exit 1); test -f build/out/melody.mid && test -f build/out/notes.mid && echo "one stop wrote melody.mid and notes.mid" || (echo "FAIL: Record off did not write melody.mid and notes.mid"; exit 1)
rm -rf build/out && ./build/host_test > build/host.log 2>&1 || { cat build/host.log; echo "FAIL: host test"; exit 1; }
g++ -std=c++14 -O2 -Isrc -c tests/edge_test.cpp -o build/edge_test.o
g++ -o build/edge_test build/edge_test.o build/velvet_interp.o build/musicout.o build/chordcore.o build/notes.o build/poly.o build/hpss.o build/keys.o -lm -lpthread
./build/edge_test > build/edge.log 2>&1 || { cat build/edge.log; echo "FAIL: edge tests"; exit 1; }
tail -1 build/edge.log
# melody path: the Line trigger must write melody.mid
g++ -std=c++14 -O2 -DVI_OUT_DIR=\"build/out_line\" -Isrc -c src/velvet_interp.cpp -o build/vi_line.o
g++ -std=c++14 -O2 -Isrc -c tests/line_test.cpp -o build/line_test.o
g++ -o build/line_test build/line_test.o build/vi_line.o build/musicout.o build/chordcore.o build/notes.o build/poly.o build/hpss.o build/keys.o -lm -lpthread
rm -rf build/out_line && mkdir -p build/out_line && ./build/line_test > /dev/null
test -f build/out_line/melody.mid && test -f build/out_line/line.mid && test -f build/out_line/notes.mid && echo "line trigger wrote line.mid, notes.mid and melody.mid"
python3 tests/check_pads.py build/out_line/melody.mid build/out_line/melody_pads.mid 36 || { echo "FAIL: melody pads"; exit 1; } || { echo "FAIL: Line did not write all three files"; exit 1; }
gcc -std=gnu99 -O2 -Wall -Wextra -Isrc tests/test_padsend.c src/padsend.c src/musicout.c src/chordcore.c -lm -o build/test_padsend
./build/test_padsend
echo "all host tests passed"
