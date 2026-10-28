# Velvet Interpolator 0.1.0: live chord analysis, pad output (MPC X plugin)

## How recording works
- Turn Record on while the sample is being recorded. Audio passing through the plugin's track is copied into a
  ring buffer (no waiting on the audio thread). A worker thread streams it into the chord analyser as it arrives.
- Turn Record off. The worker finishes the take and writes pads.txt and progression_A..D.mid right away.
- Analyse re-runs the chord analysis on the whole take. Line writes line.mid from the take.
- Input level (meter) and progress (meter and readout) are read-only; the record LED shows Record state.
- Limits: 60-second takes; if the worker falls behind, samples are dropped (counted, not reported on screen).
- The plugin must be on the audio path of the sampling input. That is not yet verified on the MPC.

## What it does
1. Records the audio that passes through the plugin (mono mix, up to 60 seconds).
2. Analyses the recording on a worker thread (the audio thread never waits).
3. Writes to `/media/az01-internal/VelvetInterp/`:
   - `pads.txt`: each chord, its pad (A01 upward), the MPC note, and the chord's MIDI notes.
   - `progression_A.mid` to `progression_D.mid`: the song's chords as pad hits in order, one file per bank.
4. Controls: Record, Analyse, Transpose (-12 to +12 semitones), Inversion (0 to 2), Progress.

The plugin does not play pads live. You import the progression files into an MPC sequence and assign sounds to the pads.

## Polyphonic note analysis (new)
- src/poly.c: finds every note in a take, including chords and overlapping lines. Method: harmonic salience per
  pitch (MIDI 36 to 96), iterative harmonic cancellation, peak picking, then note tracking.
- The Line trigger writes notes.mid (all notes) next to line.mid (single line).
- Measured on synthetic harmonic tones (onset within 50 ms, same pitch):
  tuning set F1 82.4%; held-out sets F1 81.5% (2 to 5 voices) and 83.3% (faster partial decay).
  These are synthetic results from a generator in tests/test_poly.c. Real recordings are much harder, and
  the score there is unknown.
- Speed on this machine: about 0.018 times real time (one minute in about 1 second). The MPC is much slower,
  so a long take may take tens of seconds there. Run after recording, not live.
- Known limits: octave and harmonic errors on dense or inharmonic sounds, no velocity or pitch-bend, and single
  false notes on simple lines (seen on the line test).

## Skin design (v3)
- Velvet palette: graphite chassis with gradient panels, champagne-gold lit states, silver labels, and a restrained
  violet accent on the record LED. The knobs, LED bars and background are drawn by skin_tools/make_skin.py.
- No RSFX artwork remains in the skin. Layout, sizes and parameter bindings are unchanged.
- Not verified on the MPC: how the gradients, knob artwork and fonts look on the device screen.

## Melody through drums (new)
- tools/melody.py: harmonic-percussive separation (median filtering of the spectrogram) keeps sustained tones and
  removes drum hits. The melody is then found with the monophonic YIN extractor (build/line_driver).
- Synthetic test (tests/test_melody.py, tests/line_driver.c): a single-voice melody with drums at three levels.
  YIN on the melody alone: 100% F1. With drums: raw 53% (0 dB) and 38% (+6 dB); after separation 74% and 71%.
  The polyphonic detector scores worse on the same melody (about 36% with no drums), so it is not used for melody.
- analyze_song.py writes melody.txt and melody.mid. On the uploaded song it found 191 notes (about 1 per second).
  Accuracy is unknown without an answer key.
- Limits: the separation is only as good as the spectrogram. Vocals and lead synths are harmonic, so they are kept
  and may be mistaken for the melody. Separation is in Python. The plugin's Line button does not use it yet, because
  the median filtering would need a C implementation.

## LED box (new)
- Plugin: 16 read-only band-level parameters (7 to 22), 80 Hz to 10 kHz, log spaced. Computed per sample with
  Goertzel filters on the mono input. Test: a 1 kHz tone lights band 9 (about 1050 Hz); a 100 Hz tone lights band 2.
- Skin: 16 vertical LED columns, drawn as film strips, on the right of the knob panel.
- Band centres are coarse for a 512-sample block (about 94 Hz resolution at 48 kHz), so the lowest bands are broad.
- Not verified on the MPC: update rate and smoothness of 16 film strips.

## Melody from a song, inside the plugin (new)
- src/hpss.c: harmonic-percussive separation in C (median filtering of the STFT, 2048 window, 1024 hop). It matches
  the Python version (correlation 1.000 on a clean melody).
- Line trigger now writes three files from the take: melody.mid (drums removed, then monophonic detection),
  line.mid (monophonic, drums not removed) and notes.mid (all polyphonic notes).
- Synthetic test (single-voice melody with drums, YIN note F1): drums at 0 dB: raw 53%, after separation 91%;
  drums at +6 dB: raw 38%, after separation 74%. Synthetic only.
- Cost on this computer: about 1.8 s per minute of audio, peak memory about 79 MB. The MPC is much slower, and
  memory on the device is unmeasured. Both are unverified on the MPC.
- One step: when Record goes off, the plugin writes progression_A..D.mid (one pad per chord), voiced_chords.mid
  (chord notes, with the transpose and inversion knobs applied), melody.mid and notes.mid. MIDI only, no text files. Line
  re-runs the melody and note analysis on the take.
- Rearranging: import progression_A..D.mid and melody.mid into a sequence. Whether the MPC imports them as
  intended is unverified.

## Pad features (new)
- Octave (parameter 23): sets the 16-pad window for melody pads, C2 to C6. Default C2 (MIDI 36).
- Voicing (parameter 24): Knob (uses the Inversion knob, as before), Root (close, root position), or Open.
  Applies to voiced_chords.mid. Checked for C major: root close 48 52 55, open 48 55 64, first inversion 52 55 60.
- Key (parameter 25, read-only) and Key conf (parameter 26, read-only): key estimate from the chord timeline,
  duration weighted (Krumhansl profiles). Live test on C, G, Am, F: C major, confidence 52%.
  The confidence is a margin heuristic, not a measured accuracy.
- melody_pads.mid: the melody folded by octaves into the 16-pad window set by Octave. Same note count and timing
  as melody.mid; the test checks the window and the timing.
- Not built: per-chord confidence scores; a user-facing pad map on the MPC screen.

## ARM build status
- Not compiled for ARM here: the sandbox has no ARM toolchain and no network to install one.
- Fixed from desktop checks: the plugin no longer needs libstdc++ (only libc and libm, checked with readelf on
  a desktop build); scripts/build-arm.sh now compiles and links all seven source files; the GitHub workflow compiles
  inside Ubuntu 18.04 (glibc 2.27), so the binary does not need GLIBC_2.34 symbols.
- Gate: scripts/check-arm.sh fails the build if the binary needs glibc newer than 2.27 or any library other than
  libc, libm, libpthread and the ARM loader.
- To verify: run the Build Velvet Interpolator workflow on GitHub. The build-arm job must pass every step.

## Test status (this sandbox, x86)
- Streaming chord analysis: matches the offline analysis frame for frame (169/169) with random block sizes.
- Live mode: recording a progression and turning Record off writes C, G, Am, F with no Analyse press.
- ThreadSanitizer: the host test and the edge tests run with no data races reported. Shared state is atomic.
- C music output: voicing, transposition, inversions and MIDI file header. Passing.
- Plugin driven by a fake host: records a synthetic C-Am-F-G progression, analyses it, and writes the files.
  The pad sheet gives C, G, Am, F in order and the MIDI reads back correctly with the Python reader. Passing.
- Edge cases: analyse with nothing recorded, repeated triggers, a new take, closing during analysis. Passing.
- Transposition changes the pad output (C at 48,52,55 with no shift, 60,64,67 at +12). Passing.
- Installer and uninstaller on a fake MPC tree. Passing, with a placeholder skin used in the test only.

## Line extraction (new)
- Monophonic note extraction (YIN pitch detection, 40 to 1000 Hz, downsampled to 12 kHz).
- Plugin control "Line": analyses the recording as a single line and writes `line.mid`.
- Measured on 40 random synthetic lines (bass and lead): precision 95.1%, recall 96.0%, pitch correct on
  100% of matched notes, mean onset error 7.9 ms. Synthetic audio only.
- Speed on this machine: about 0.011 times real time (one minute of audio in about 0.7 s). The MPC is
  much slower, so this is unmeasured there.
- Limits: one note at a time. Repeated notes are split only where the level dips between them.

## NOT done or NOT verified
- **Screen skin: written, not seen on the MPC.** `skin_tools/make_skin.py` writes the skin (TUI.json, Q-Links.json
  and PNG images) from the same structure as Velvet Chords' skin. `skin_tools/test_skin.py` checks it: image sizes,
  bounds, parameter bindings and segment groups all pass. Controls: Record (P0), Analyse (P1), Transpose (P2, knob), Inversion (P3, knob),
  Line (P5). LED screen: a record LED (P0), a horizontal LED bar for input level (P6, -60 to 0 dB) with a readout, and a
  horizontal LED bar for analysis progress (P4) with a readout. Progress and Level are read-only. Transpose and Inversion use RSFX's knob film strip and labels.
  Unverified: how the MPC draws it, the knob drag behaviour, the Titillium Web font on the MPC,
  the Q-Link mapping, and whether /sdcard/Synths is in MPC's SynthContentLocations (the installer warns if not).
- **ARM build: not compiled.** The workflow cross-compiles it on GitHub; it hasn't run yet.
- **Device gate: not run.** check-arm.sh now also allows libpthread.so.0 (glibc 2.27 keeps pthreads separately).
- **On the MPC:** audio reaching the plugin while recording, the output folder, pthread behaviour, memory, and
  the time an analysis takes. None has been measured on the device.
- **Pad mapping:** pad A01 = note 36 and the bank layout are assumed. Check them on the unit.
- **Interpolation:** only transposition and inversion are exposed in the plugin. Open voicing exists in the C code but has no control.
- **Accuracy on real recordings:** unknown. Every test used synthetic tones.
- **Memory:** a 60-second mono buffer is about 11.5 MB at 48 kHz and about 23 MB at 96 kHz.

## Build
GitHub workflow `.github/workflows/build.yml`: runs the host tests, cross-compiles velvet_interp.so for ARMv7,
and checks the binary against the device gate.

## Analysing real songs
    python3 tools/analyze_song.py song.mp3 OUT_DIR [--answer chords.txt]
- Reads any format ffmpeg reads (MP3, M4A, OGG, FLAC, WAV). Needs ffmpeg installed.
- Writes chords.mid, notes.mid and melody.mid into OUT_DIR (MIDI only).
- --answer scores the chords against a hand-written answer file (START END CHORD per line). Reading the answer file is the only text the tools use.
- Build the poly driver first: gcc -O2 -Isrc tests/poly_driver.c src/poly.c -lm -o build/poly_driver
- Tested on a known progression encoded as M4A and OGG: chords 100% correct. Polyphonic notes over-detect on
  sustained chords (12 and 19 notes for a 4-chord progression).
- Not tested on a real commercial song. Dense mixes (drums, vocals, reverb) are expected to score much lower.
  There is no source separation, so the drums and vocals are analysed along with everything else.

## MIDI out to pads (new, not verified on the MPC)
- The plugin declares MIDI output (effCanDo "sendVstMidiEvent"). While the host transport plays, each pad hit from
  the last analysis goes out as a note-on and note-off, counted from the moment the transport started.
- Bank A to D is sent on MIDI channels 1 to 4; the pad note is 36 + index % 16. Tempo is fixed at 120 BPM, so the
  hits stay in time only if the MPC project runs at 120 BPM.
- When the transport stops, every note gets a note-off.
- Tested with a fake host (tests/midi_out_test.cpp): hits go out while playing, note-offs match, a stop sends
  note-offs, and a restart replays from zero. Not tested: whether the MPC accepts MIDI from this plugin, what the
  MPC does with the channels, or whether the transport position it reports is usable. Those need the device.
