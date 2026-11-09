# The built-in synth: plain C++, no JUCE, so it is unit-tested like the core.
add_library(maqam_synth STATIC
    Drone.cpp
    KeyTuning.cpp
    TempoSync.cpp
    Synth.cpp
)

target_link_libraries(maqam_synth PUBLIC maqam_core)
target_compile_features(maqam_synth PUBLIC cxx_std_17)
set_target_properties(maqam_synth PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(maqam_synth PRIVATE /W4)
else()
    target_compile_options(maqam_synth PRIVATE -Wall -Wextra -Wpedantic)
endif()
