# Jazz Box. Plugin code Jzbx must never change, or Live will lose it in saved projects.
juce_add_plugin(JazzBox
    COMPANY_NAME "Pibox"
    PLUGIN_MANUFACTURER_CODE Pibx
    PLUGIN_CODE Jzbx
    FORMATS AU VST3
    PRODUCT_NAME "Jazz Box"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(JazzBox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(JazzBox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(JazzBox
    PRIVATE boxkit juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline render tool for measuring this box's DSP
juce_add_console_app(JazzBoxRender PRODUCT_NAME "JazzBoxRender")
target_sources(JazzBoxRender PRIVATE Tools/Render.cpp)
target_compile_definitions(JazzBoxRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(JazzBoxRender
    PRIVATE boxkit juce::juce_audio_formats juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
