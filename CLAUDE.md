# Maqam Master

Microtonal maqam instrument/teacher. JUCE (C++17+), CMake, VST3/AU + standalone. See PLAN.md for scope and phases.

## Rules
- `data/maqams.json` is the single source of truth for maqam data. Never hardcode scales in C++ or UI code.
- All tuning is in cents from the tonic. Convert to Hz in the core library; synth voices take a frequency, not MIDI pitch bend.
- Core library (`src/core`) has no JUCE UI dependencies and must have unit tests (`tests/`).
- Default tonic is F (MIDI 65 = F4). Tonic must be selectable.
- Maqam data marked `"review": "needed"` has not been verified by a player/teacher; don't present it as authoritative in the UI.
- Keep v1.0 scope: Explorer, Jins Library, illuminated keyboard, Play Mode, Builder, built-in synth, Ableton tuning bridge. No Learn Mode or rhythm machine until v1.0 ships.

## Build/test
- Configure: `cmake -B build`
- Build: `cmake --build build`
- Test: `ctest --test-dir build`
- Fast core-only build (no JUCE download): `cmake -B build-core -G Ninja -DMAQAM_BUILD_PLUGIN=OFF && cmake --build build-core && ctest --test-dir build-core`
- Listen to the tuning: `build/tools/render_scales scales` (add `--reference-tonic` for traditional tonics)
- Listen to the built-in synth: `build/tools/render_synth` (writes `scales/synth/`; `--sound pluck|synth|ney`, `--vibrato 0..1`, `--legato GLIDE_S`)
- Install the plugin for Ableton: copy `build/src/plugin/MaqamMaster_artefacts/Debug/VST3/Maqam Master.vst3` to `~/Library/Audio/Plug-Ins/VST3/`, then restart Live.
- Built-in synth lives in `src/synth` (no JUCE, tested in `tests/test_synth.cpp`): every voice takes an exact frequency from a `KeyTuning`, handed to the audio thread through `TuningExchange`.
- In plugin files, include JUCE headers before any standard header: `<array>` first makes JUCE's macOS SDK check fail.
- Export tuning files for Ableton Live 12 / Scala synths: `build/tools/export_tunings tunings` (`.ascl`, `.scl`, `.kbm`, in 12-key and one-degree-per-key layouts)
- Teacher review page: `review/index.html`, published as https://claude.ai/artifact/R2Q47XQiXA35gdh8bNKNVU. After changing `data/maqams.json`, run `build/tools/export_review_data` and republish with `review/review-data.json`. Answers are in the page's `reviews` collection, one doc per maqam per reviewer.
- CMake/Ninja are installed via `pip3 install --user cmake ninja`; add `~/Library/Python/3.9/bin` to PATH.
- Dependencies (JUCE, Catch2, nlohmann/json) are pinned in the root `CMakeLists.txt` and fetched by CMake.
