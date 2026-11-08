add_executable(maqam_tests
    test_data.cpp
    test_tuning.cpp
    test_render.cpp
    test_keymapping.cpp
    test_notenames.cpp
    test_synth.cpp
)

target_link_libraries(maqam_tests PRIVATE maqam_synth Catch2::Catch2WithMain)
target_compile_definitions(maqam_tests PRIVATE MAQAM_DATA_FILE="${MAQAM_DATA_FILE}")

include(Catch)
catch_discover_tests(maqam_tests)
