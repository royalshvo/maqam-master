# UI-free core library: maqam data model, JSON loader, tuning math.
# Must not depend on JUCE.
add_library(maqam_core STATIC
    MaqamData.cpp
    Tuning.cpp
    Render.cpp
    KeyMapping.cpp
)

target_include_directories(maqam_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(maqam_core PRIVATE nlohmann_json::nlohmann_json)
target_compile_features(maqam_core PUBLIC cxx_std_17)
set_target_properties(maqam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(maqam_core PRIVATE /W4)
else()
    target_compile_options(maqam_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
