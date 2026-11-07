add_executable(render_scales render_scales.cpp)
target_link_libraries(render_scales PRIVATE maqam_core)
target_compile_definitions(render_scales PRIVATE MAQAM_DATA_FILE="${MAQAM_DATA_FILE}")
