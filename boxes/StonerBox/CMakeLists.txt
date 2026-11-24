# Stoner Box. Plugin code Stbx must never change, or Live will lose it in saved projects.
juce_add_plugin(StonerBox
    COMPANY_NAME "Pibox"
    PLUGIN_MANUFACTURER_CODE Pibx
    PLUGIN_CODE Stbx
    FORMATS AU VST3
    PRODUCT_NAME "Stoner Box"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(StonerBox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(StonerBox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(StonerBox
    PRIVATE boxkit juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline render tool for measuring this box's DSP
juce_add_console_app(StonerBoxRender PRODUCT_NAME "StonerBoxRender")
target_sources(StonerBoxRender PRIVATE Tools/Render.cpp)
target_compile_definitions(StonerBoxRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(StonerBoxRender
    PRIVATE boxkit juce::juce_audio_formats juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
