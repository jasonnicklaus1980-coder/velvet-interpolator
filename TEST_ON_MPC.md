# Velvet Interpolator: device test checklist

Before this checklist: the Build Velvet Interpolator workflow on GitHub must pass its build-arm job, including the
check-arm gate. Download the artifact from that run.

Run these in order. Stop at the first failure and note what you saw.

1. **Install.** Build the package with the GitHub workflow, download the artifact, copy it to the MPC, unzip,
   then run `sh VelvetInterpolator-0.1.0/install.sh` as root. Expected: "Installed Velvet Interpolator".
   If it stops with "screen skin missing", the package is wrong. Tell me the message.
2. **Plugin appears.** Add Velvet Interpolator from the plugin browser on a track.
3. **Skin appears.** Check the layout: toggles, Transpose and Inversion knobs, level and progress meters, record LED.
4. **Routing (the key question).** Set the track input to the source you sample from, play something into it.
   The Level meter should move. If it stays at zero, the plugin is not hearing the input.
   Then try: sample first, play the sample back through this track, and watch the Level meter.
5. **One stop.** Turn Record on, play a simple progression (for example C, G, A minor, F, two bars each), with a
   drum loop underneath if you have one. Turn Record off, and wait. Expected in /media/az01-internal/VelvetInterp:
   progression_A.mid, voiced_chords.mid, melody.mid and notes.mid. The progress meter goes to 100% when they're done.
6. **Pads and voicing.** Set Octave to C4 and check that melody_pads.mid plays inside the 16 pads from C4. Set
   Voicing to Root, then Open, and compare voiced_chords.mid. Read the Key readout after a stop: it should match
   the key of the progression you played.
7. **Line (optional).** Press Line to re-run the melody and note analysis on the take.
8. **Load a pad.** Import progression_A.mid into a sequence and check that each pad plays one chord. Play
   voiced_chords.mid through a keyboard or synth and turn the Transpose and Inversion knobs to hear the change.

Send me: the step that failed, and a screenshot or message of what you saw.
