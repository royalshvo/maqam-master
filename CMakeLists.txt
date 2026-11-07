cmake_minimum_required(VERSION 3.22)

project(MaqamMaster VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

option(MAQAM_BUILD_PLUGIN "Build the JUCE plugin and standalone app (downloads JUCE)" ON)
option(MAQAM_BUILD_TESTS  "Build unit tests" ON)
option(MAQAM_BUILD_TOOLS  "Build command-line tools (WAV scale renderer)" ON)

set(MAQAM_DATA_FILE "${CMAKE_CURRENT_SOURCE_DIR}/data/maqams.json")

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# Pinned dependency versions.
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(nlohmann_json)

add_subdirectory(src/core)

if(MAQAM_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(MAQAM_BUILD_TESTS)
    FetchContent_Declare(Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(Catch2)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MAQAM_BUILD_PLUGIN)
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(src/plugin)
endif()
