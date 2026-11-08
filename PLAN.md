# Maqam Master – Learn • Play • Explore

A microtonal maqam instrument and teacher, as a JUCE VST3/AU plugin with a **built-in sound engine** (demo and practice from day one), plus tuning/MIDI bridges for Ableton.

## Product pillars
1. **Maqam Explorer** – pick a maqam, see its structure (ajnas, tonic, ghammaz, ascending/descending forms).
2. **Jins Library** – every jins defined once, and every maqam decomposed into ajnas.
3. **Illuminated Keyboard** – colors for tonic, characteristic notes, quarter-tones, passing notes.
4. **Play Mode** – play freely; plugin highlights and snaps to the selected maqam. Qanun-style controls:
   - **Mandal switches** – one lever per note (pitch class), like the mandals on a qanun: flip it during a performance to raise or lower that note in fixed steps (quarter tone for Arabic, comma steps for Turkish, set by the tuning preset). Playable from the UI, MIDI CC or keyswitches. Also used for the ascending/descending change (e.g. Rast's B half-flat ↔ B♭) and for switching to another maqam or jins live.
   - **Bend knob** – pitch bend with a selectable range in cents (e.g. ±50 / ±100 / ±200) and an optional maqam-aware mode that bends to the next maqam degree instead of a fixed interval.
5. **Maqam Builder** – combine lower and upper ajnas (including overlapping ajnas).
6. **Built-in sound engine** – simple, good-sounding synth with microtonal tuning, for demos, practice and lessons.
7. **Learn Mode** – guided lessons per jins (listen, play along, quiz).
8. **Rhythm machine** – step sequencer with Arabic and trance presets.
9. **Ableton integration** – Scala/Live tuning export, MIDI out (MPE or plain notes), MIDI clip export, and an MTS-ESP master.

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
  2. **MIDI out from the plugin**, two modes:
     - *MPE*: one channel per note with its own pitch bend, so quarter tones, mandal flips and the bend knob all reach the synth live, and chords can mix tuned and untuned notes. Works with MPE synths (Live's Wavetable, Drift, Meld, Sampler; Vital, Surge XT, Pigments, Equator…).
     - *Plain notes*: normal MIDI notes; the receiving synth supplies the tuning (`.ascl`/`.scl` or MTS-ESP).
     In Live, the receiving track sets **MIDI From** to the plugin's track. VST3 only on Mac (AU plugins generally can't send MIDI out in Live).
  3. **MIDI clip export**: record what you play and drag it out as a MIDI file, either with MPE bends or as plain notes plus the matching `.ascl`.
  4. **MTS-ESP master** (library is 0BSD; confirm libMTS distribution terms with ODDSound): lets non-MPE synths follow live retuning (mandals, maqam switches). Only one master can be active at a time.
  - Dropped: the separate Max for Live device. MIDI out + MPE covers it, and Max for Live needs Live Suite (Standard doesn't have it). Revisit only if testing shows a gap.

## Data model (`data/maqams.json`)
- Single source of truth for the plugin and every export. Conventions are documented in the file's `conventions` block.
- Jins intervals are defined once in a top-level `ajnas` library. Each maqam lists `{jins, root_degree}`; the core derives absolute cents.
- A unit test checks that every maqam's `ascending` scale (and each variant and descending form) matches its ajnas.
- Ascending variants and descending forms are `{changes, ajnas}`. `descending: null` means "not yet specified": the UI must not show one.
- Scales may be non-octave-repeating (`octave_repeating: false`, e.g. Saba).
- Quarter-tone status is **not stored**: it depends on the selected tonic, so the core computes it.
- Tuning presets carry per-maqam, per-degree cent offsets (Arab/Turkish variants, reviewer corrections).

## Key design decisions (settle in Phase 1)
- **Keyboard mapping — decided: 12-key layout is the default.** Every chromatic key keeps its place; the key nearest each maqam note is retuned to it (a half-flat goes on the upper key, so Rast on F puts A half-flat on the A key). Keys outside the maqam play 12-TET for now. The one-degree-per-key layout stays available as an option. Implemented in `src/core/KeyMapping` and exported to `.ascl`/`.scl`/`.kbm`; the Rast `.ascl` was checked by ear in Ableton Live 12. Out-of-maqam keys: a **Keys** switch in the built-in synth: *All keys* (default, 12-TET outside the maqam), *Mute outside*, or *Snap to maqam* (nearest maqam note; ties go to the lower one). Snap makes a Launchpad in chromatic Note mode play only maqam notes.
- **Live retuning:** mandal switches and the bend knob change pitch while notes play. The built-in synth follows instantly (per-voice frequency). External synths in Live only follow live changes through MTS-ESP; a static `.ascl` can't. So mandals strengthen the case for the MTS-ESP master in Phase 4. The core `TuningTable` must support per-step offsets that change at runtime, without allocating on the audio thread.
- **Note spelling after transposition:** a naming system with half-flats/half-sharps for any selected tonic, since reference names (e.g. "E half-flat (Sikah)") only hold at the reference tonic.
- **Reference pitch:** A4 configurable (default 440 Hz).
- **Plugin state:** tonic, maqam, variant, tuning preset and synth settings saved in the DAW project.

## Phases

| # | Phase | Deliverable | Est. |
|---|---|---|---|
| 0 | Data | `maqams.json` for 8 maqamat, reviewed by a player/teacher. **Runs in parallel with Phases 1–3**; engineering works on draft data. | ongoing |
| 1 | Core engine | maqam/jins model, tuning math, note-role lookup, keyboard mapping, note spelling, Builder logic, Scala/`.ascl` export, unit tests | 2–3 wks |
| 2 | Sound engine | polyphonic microtonal synth, plays any maqam from the core. **Mostly done:** `src/synth` with three sounds (Pluck = Karplus-Strong, Synth = saw+square → filter, Ney = breathy flute), 16 voices, exact per-key frequency, held notes follow retunes; Keys limit (all/mute/snap); Poly/Mono/Legato with glide that lands exactly on the maqam pitch; vibrato (amount + mod wheel, delayed fade-in, centred so the average pitch stays in tune). Plugin has sound controls, reverb and an illuminated keyboard; checked in Ableton Live 12. Still to do: chorus, sound polish, pitch wheel as the bend knob. | 2–3 wks |
| 3 | Playable plugin UI | Explorer, Jins Library, illuminated keyboard, Play Mode (mandal switches, bend knob), Builder, state save/restore | 5–7 wks |
| 4 | Ableton bridge | MIDI out (MPE + plain notes), MIDI clip export, MTS-ESP master; test routing in Live and with the synths you use | 3–4 wks |
| 5 | Beta + release | signing/notarization and installers (needed for Mac testers), testers, bug fixes, presets, **v1.0** | 4–5 wks |
| 6 | Launch | website, demo video, store/payment | 2–3 wks |
| 7 | Learn Mode | lessons for 3 maqamat first, then expand (**v1.1**) | 4–6 wks |
| 8 | Rhythm machine | step sequencer, Arabic + trance presets, transport sync (**v1.2**) | 2–3 wks |

## Milestones
- M1: Hijaz on tonic F: unit tests assert the computed frequencies for every degree; a WAV render is for listening.
- M2: play the keyboard in the standalone app with live note highlighting.
- M3: load the VST3 in Ableton and play maqam-accurate quarter tones. **Reached** (Live 12 Standard, built-in synth).
- M4: another synth follows your tuning (Live 12 `.ascl` first, then MPE MIDI out and MTS-ESP). **`.ascl` part reached:** Live's instruments play the exported Rast tuning.

## Risks
- **Tuning accuracy/tradition differences:** offer Arab/Turkish presets and per-degree cent offsets; credit reviewers.
- **Reviewer availability:** finding and scheduling a player/teacher is its own risk; start now, don't block engineering on it.
- **Licensing:** check JUCE (AGPLv3 or commercial tiers), VST3 SDK and MTS-ESP terms **before Phase 1**, not before release.
- **Ableton routing:** Live can take a plugin's MIDI output on another track (MIDI From), but this is untested for this plugin as VST3, and AU MIDI out generally doesn't work in Live. Test early in Phase 4; the built-in engine, `.ascl` export and MTS-ESP don't depend on it.
- **MPE compatibility:** synths without MPE play MPE output wrongly, so MPE stays an opt-in mode and plain notes + tuning is the default.
- **Scope creep:** ship v1.0 without Learn Mode and rhythm machine.
- **Support:** Mac/Windows builds, DAW differences.

## First week (Claude Code tasks)
1. `git init`, then scaffold CMake + JUCE project, with a standalone target and a test target.
2. Define the C++ data model and JSON loader for `data/maqams.json`, including the ajnas library, variants, descending forms and tuning presets.
3. Add the data-consistency test (ascending scale vs. ajnas, ghammaz is a jins root).
4. Implement cents → frequency, tonic transposition (default tonic F, A4 configurable), quarter-tone detection and note-role lookup, with tests.
5. Render a short audio test of each maqam scale to WAV to listen to the tuning.
