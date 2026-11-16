juce_add_plugin(MaqamMaster
    PRODUCT_NAME "Maqam Master"
    COMPANY_NAME "Maqam Master"
    BUNDLE_ID "com.maqammaster.plugin"
    PLUGIN_MANUFACTURER_CODE Mqmm
    PLUGIN_CODE Mqm1
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# The plugin carries its own copy of the data file so it never depends on a path on disk.
juce_add_binary_data(MaqamBinaryData SOURCES ${MAQAM_DATA_FILE})

target_sources(MaqamMaster PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(MaqamMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MaqamMaster
    PRIVATE
        maqam_synth
        MaqamBinaryData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Developer tool: draws the plugin window to a PNG, so the layout can be checked without a host.
#   build/src/plugin/render_ui_artefacts/Debug/render_ui out.png [maqam_id] [variant_id]
juce_add_console_app(render_ui PRODUCT_NAME "render_ui")
target_sources(render_ui PRIVATE render_ui.cpp PluginProcessor.cpp PluginEditor.cpp)
target_compile_definitions(render_ui PRIVATE
    JucePlugin_Name="Maqam Master"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries(render_ui PRIVATE maqam_synth MaqamBinaryData juce::juce_audio_utils)
