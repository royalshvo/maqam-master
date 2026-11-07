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
- CMake/Ninja are installed via `pip3 install --user cmake ninja`; add `~/Library/Python/3.9/bin` to PATH.
- Dependencies (JUCE, Catch2, nlohmann/json) are pinned in the root `CMakeLists.txt` and fetched by CMake.
