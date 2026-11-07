# Maqam Master – Learn • Play • Explore

A microtonal maqam instrument and teacher, as a JUCE VST3/AU plugin with a **built-in sound engine** (demo and practice from day one), plus tuning/MIDI bridges for Ableton.

## Product pillars
1. **Maqam Explorer** – pick a maqam, see its structure (ajnas, tonic, ghammaz, ascending/descending forms).
2. **Jins Library** – every jins defined once, and every maqam decomposed into ajnas.
3. **Illuminated Keyboard** – colors for tonic, characteristic notes, quarter-tones, passing notes.
4. **Play Mode** – play freely; plugin highlights and snaps to the selected maqam.
5. **Maqam Builder** – combine lower and upper ajnas (including overlapping ajnas).
6. **Built-in sound engine** – simple, good-sounding synth with microtonal tuning, for demos, practice and lessons.
7. **Learn Mode** – guided lessons per jins (listen, play along, quiz).
8. **Rhythm machine** – step sequencer with Arabic and trance presets.
9. **Ableton integration** – Scala/Live tuning export, MTS-ESP master and/or Max for Live MIDI+pitch-bend companion.

## Release scope
- **v1.0:** pillars 1–6 + tuning bridge (Ableton integration), signed and notarized installers
- **v1.1:** Learn Mode
- **v1.2:** Rhythm machine

## Tech stack
- C++17/20, JUCE, CMake. Plugin formats: VST3 + AU (Mac), VST3 (Windows).
- Core engine as a UI-free static library with unit tests (Catch2 or doctest).
- Version control: git from the first commit.
- Sound engine: polyphonic synth, per-voice frequency from the tuning table (not MIDI pitch bend). Start with 2 oscillators + filter + amp envelope + simple chorus/reverb, plus a plucked (oud-like, e.g. Karplus-Strong) mode.
- Tuning bridge, in order of cost:
  1. Export Scala (`.scl` + `.kbm`) and Ableton Live 12 tuning (`.ascl`) files from the core library.
  2. MTS-ESP master (check licensing and which synths are clients).
  3. Max for Live device sending MIDI + per-note pitch bend (MPE). Requires Live Suite or Max for Live.

## Data model (`data/maqams.json`)
- Single source of truth, shared by the plugin and the Max for Live companion. Conventions are documented in the file's `conventions` block.
- Jins intervals are defined once in a top-level `ajnas` library. Each maqam lists `{jins, root_degree}`; the core derives absolute cents.
- A unit test checks that every maqam's `ascending` scale (and each variant and descending form) matches its ajnas.
- Ascending variants and descending forms are `{changes, ajnas}`. `descending: null` means "not yet specified": the UI must not show one.
- Scales may be non-octave-repeating (`octave_repeating: false`, e.g. Saba).
- Quarter-tone status is **not stored**: it depends on the selected tonic, so the core computes it.
- Tuning presets carry per-maqam, per-degree cent offsets (Arab/Turkish variants, reviewer corrections).

## Key design decisions (settle in Phase 1)
- **Keyboard mapping:** how a 12-key MIDI keyboard plays a maqam. Options: (a) retune each chromatic key to the nearest maqam degree, (b) map scale degrees to white keys only. Decide what out-of-maqam keys do (mute, snap, or play unchanged). This drives Play Mode, the illuminated keyboard and the bridge export (`.kbm`).
- **Note spelling after transposition:** a naming system with half-flats/half-sharps for any selected tonic, since reference names (e.g. "E half-flat (Sikah)") only hold at the reference tonic.
- **Reference pitch:** A4 configurable (default 440 Hz).
- **Plugin state:** tonic, maqam, variant, tuning preset and synth settings saved in the DAW project.

## Phases

| # | Phase | Deliverable | Est. |
|---|---|---|---|
| 0 | Data | `maqams.json` for 8 maqamat, reviewed by a player/teacher. **Runs in parallel with Phases 1–3**; engineering works on draft data. | ongoing |
| 1 | Core engine | maqam/jins model, tuning math, note-role lookup, keyboard mapping, note spelling, Builder logic, Scala/`.ascl` export, unit tests | 2–3 wks |
| 2 | Sound engine | polyphonic microtonal synth, plays any maqam from the core | 2–3 wks |
| 3 | Playable plugin UI | Explorer, Jins Library, illuminated keyboard, Play Mode, Builder, state save/restore | 4–6 wks |
| 4 | Ableton bridge | MTS-ESP master and/or M4L companion; test with the synths you use | 2–3 wks |
| 5 | Beta + release | signing/notarization and installers (needed for Mac testers), testers, bug fixes, presets, **v1.0** | 4–5 wks |
| 6 | Launch | website, demo video, store/payment | 2–3 wks |
| 7 | Learn Mode | lessons for 3 maqamat first, then expand (**v1.1**) | 4–6 wks |
| 8 | Rhythm machine | step sequencer, Arabic + trance presets, transport sync (**v1.2**) | 2–3 wks |

## Milestones
- M1: Hijaz on tonic F: unit tests assert the computed frequencies for every degree; a WAV render is for listening.
- M2: play the keyboard in the standalone app with live note highlighting.
- M3: load the VST3 in Ableton and play maqam-accurate quarter tones.
- M4: another synth follows your tuning (Live 12 `.ascl` first, then MTS-ESP or M4L).

## Risks
- **Tuning accuracy/tradition differences:** offer Arab/Turkish presets and per-degree cent offsets; credit reviewers.
- **Reviewer availability:** finding and scheduling a player/teacher is its own risk; start now, don't block engineering on it.
- **Licensing:** check JUCE (AGPLv3 or commercial tiers), VST3 SDK and MTS-ESP terms **before Phase 1**, not before release.
- **Ableton routing:** VSTs can't send MIDI to other tracks in Live; hence built-in engine + tuning export + MTS-ESP/M4L.
- **Scope creep:** ship v1.0 without Learn Mode and rhythm machine.
- **Support:** Mac/Windows builds, DAW differences.

## First week (Claude Code tasks)
1. `git init`, then scaffold CMake + JUCE project, with a standalone target and a test target.
2. Define the C++ data model and JSON loader for `data/maqams.json`, including the ajnas library, variants, descending forms and tuning presets.
3. Add the data-consistency test (ascending scale vs. ajnas, ghammaz is a jins root).
4. Implement cents → frequency, tonic transposition (default tonic F, A4 configurable), quarter-tone detection and note-role lookup, with tests.
5. Render a short audio test of each maqam scale to WAV to listen to the tuning.
