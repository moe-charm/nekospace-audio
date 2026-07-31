cmake_minimum_required(VERSION 3.22)
project(NekoSpaceBinaural VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE (pinned tag — Architecture Contract: never track master) ---
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- JUCE-free DSP core (Contract #4) ---
add_library(nsb_dsp INTERFACE)
target_include_directories(nsb_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- Plugin ---
juce_add_plugin(NekoSpaceBinaural
    COMPANY_NAME "TextureVoice"
    PLUGIN_MANUFACTURER_CODE Txvc
    PLUGIN_CODE Nksb
    FORMATS VST3 Standalone
    PRODUCT_NAME "NekoSpace Binaural"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Spatial"
)

target_sources(NekoSpaceBinaural PRIVATE
    src/plugin/PluginProcessor.cpp
)

target_compile_definitions(NekoSpaceBinaural PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NekoSpaceBinaural
    PRIVATE
        nsb_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(NekoSpaceBinaural PRIVATE /utf-8)
endif()

# --- JUCE-free DSP tests (Contract #12) ---
enable_testing()
add_executable(nsb_tests tests/DspTests.cpp)
target_link_libraries(nsb_tests PRIVATE nsb_dsp)
if(MSVC)
    target_compile_options(nsb_tests PRIVATE /W4 /utf-8)
endif()
add_test(NAME dsp COMMAND nsb_tests)
