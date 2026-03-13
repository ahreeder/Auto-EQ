cmake_minimum_required(VERSION 3.22)
project(PaAutoEQ VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PaAutoEQ
    COMPANY_NAME                "LiveSoundTools"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    PLUGIN_MANUFACTURER_CODE    Lst1
    PLUGIN_CODE                 PaEq
    FORMATS                     VST3
    PRODUCT_NAME                "PA Auto EQ"
)

juce_generate_juce_header(PaAutoEQ)

target_sources(PaAutoEQ PRIVATE
    Source/BiquadFilter.cpp
    Source/EQBank.cpp
    Source/FFTAnalyzer.cpp
    Source/TargetCurve.cpp
    Source/ConvergenceEngine.cpp
    Source/SpectrumDisplay.cpp
    Source/BandEditorPanel.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(PaAutoEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PaAutoEQ
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
