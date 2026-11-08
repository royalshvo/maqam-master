foreach(tool render_scales export_tunings export_review_data render_synth)
    add_executable(${tool} ${tool}.cpp)
    target_compile_definitions(${tool} PRIVATE MAQAM_DATA_FILE="${MAQAM_DATA_FILE}")
endforeach()
foreach(tool render_scales export_tunings export_review_data)
    target_link_libraries(${tool} PRIVATE maqam_core)
endforeach()
target_link_libraries(export_review_data PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(render_synth PRIVATE maqam_synth)  # brings maqam_core with it
